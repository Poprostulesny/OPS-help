#include "risk.h"
typedef struct player
{
    region_t *board;
    int num_regions;
    pthread_mutex_t *board_mut;
    char name;
    int *end_game;
    pthread_mutex_t *end_game_mut;
      pthread_mutex_t* quit_mut;
    int *quit;
} player_t;
typedef struct signal{
    pthread_mutex_t* quit_mut;
    int *quit;
    region_t *board;
    int num_regions;
    pthread_mutex_t *board_mut;
    sigset_t *mask;
}signal_t;
void usage(int argc, char **argv)
{
    fprintf(stderr, "USAGE: %s levelname.risk\n", argv[0]);
    exit(EXIT_FAILURE);
}
void *signal_handler(void * args){
    signal_t *arg = args;
    int * quit = arg->quit;
    int   num_regions = arg->num_regions; 
    pthread_mutex_t * board_mut = arg->board_mut;
    pthread_mutex_t * quit_mut = arg->quit_mut;
   region_t * board = arg->board;
   sigset_t *mask = arg->mask;
  unsigned int seed = time(NULL)^pthread_self();
   while(1){
        pthread_mutex_lock(quit_mut);
        if (*quit)
        {           
            pthread_mutex_unlock(quit_mut);
            return NULL;
        }
        pthread_mutex_unlock(quit_mut);
        int sig;
        sigwait(mask,&sig );
        switch(sig){
            case SIGINT:
               int done = 1;
               while(done){
                    int r = rand_r(&seed)%num_regions;
                    pthread_mutex_lock(&board_mut[r]);
                    if(board[r].owner != '-'){
                        board[r].owner =  '-';
                        done =0;
                    }
                    pthread_mutex_unlock(&board_mut[r]);
               }
            break;
            case SIGTERM:
               pthread_mutex_lock(quit_mut);
               *quit = 1;
               pthread_mutex_unlock(quit_mut);
               break;
        }
   }

}
void *player(void *args)
{
    player_t *arg = args;
    region_t *board = arg->board;
    pthread_mutex_t *board_mut = arg->board_mut;
    int *end_game = arg->end_game;
    pthread_mutex_t *end_game_mut = arg->end_game_mut;
    int num_regions = arg->num_regions;
    char name = arg->name;
    pthread_mutex_t *quit_mut = arg->quit_mut;
    int * quit = arg->quit;
    unsigned int seed = time(NULL) ^ pthread_self();
    int frustration = 0;
    while (frustration < FRUSTRATION_LIMIT)
    {   
        int new_pos = rand_r(&seed) % num_regions;
        pthread_mutex_lock(quit_mut);
        if (*quit)
        {
           
            pthread_mutex_unlock(quit_mut);
            return NULL;
        }
        pthread_mutex_unlock(quit_mut);
        
        int acc = 0;
        for (int i = 0; i < board[new_pos].num_neighbors; i++)
        {

            int ind = board[new_pos].neighbors[i];
            pthread_mutex_lock(&board_mut[ind]);
            if (board[ind].owner == name)
            {
                acc = 1;
                pthread_mutex_unlock(&board_mut[ind]);
                break;
            }
            pthread_mutex_unlock(&board_mut[ind]);
        }

        pthread_mutex_lock(&board_mut[new_pos]);
        
        if (board[new_pos].owner == name)
        {
            frustration++;
            printf("I, king %c, weren't able to claim the %dth region. It is already my field.\n", name, new_pos);
            pthread_mutex_unlock(&board_mut[new_pos]);
            continue;
        }
        
        if (acc == 1)
        {
            
            board[new_pos].owner = name;
            
            frustration = 0;
            printf("I, king %c, were able to claim the %dth region.\n", name, new_pos);
        }
        else
        {
            frustration++;
            printf("I, king %c, weren't able to claim the %dth region. I have no neighboring region.\n", name, new_pos);
        }
        ms_sleep(MOVE_MS);
        pthread_mutex_unlock(&board_mut[new_pos]);
    }
    if(frustration>=FRUSTRATION_LIMIT){
        printf("King %c has given up.\n", name);
    }
    pthread_mutex_lock(end_game_mut);
    *end_game = 1;
    pthread_mutex_unlock(end_game_mut);

    return NULL;
}
int main(int argc, char **argv)
{   
    if (argc != 2)
    {
        usage(argc, argv);
    }
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask, &oldmask);
    int quit = 0;
    pthread_mutex_t quitter = PTHREAD_MUTEX_INITIALIZER;
    int num_regions;
    region_t *board = load_regions(argv[1], &num_regions);
    unsigned int seed = time(NULL) ^ getpid();
    int starting_point = rand_r(&seed) % num_regions;
    int starting_point2 = rand_r(&seed) % num_regions;
    if (starting_point2 == starting_point)
    {
        starting_point2 = (starting_point2 + 1) % num_regions;
    }
    board[starting_point2].owner = 'B';
    board[starting_point].owner = 'A';
    pthread_mutex_t *board_mut = malloc(num_regions * sizeof(*board_mut));
    if (board_mut == NULL)
    {
        ERR("malloc");
    }
    int err = 0;
    for (int i = 0; i < num_regions; i++)
    {
        err = err | pthread_mutex_init(&board_mut[i], NULL);
    }
    if (err != 0)
    {
        ERR("pthread_mutex_init");
    }
    pthread_mutex_t end_game_mut = PTHREAD_MUTEX_INITIALIZER;
    int end_game = 0;
    player_t arg1 = {board, num_regions, board_mut, 'A', &end_game, &end_game_mut, &quitter, &quit};
    player_t arg2 = {board, num_regions, board_mut, 'B', &end_game, &end_game_mut, &quitter, &quit};
    signal_t args = {&quitter, &quit,board, num_regions, board_mut, &oldmask};
    pthread_t player1;
    pthread_t player2;
    pthread_t signal;
    pthread_create(&signal, NULL, signal_handler, &args);
    pthread_create(&player1, NULL, player, &arg1);
    pthread_create(&player2, NULL, player, &arg2);
    int flag = 2;
    int pointsa=0;
    int pointsb=0;
    while (flag>0)
    {   
        ms_sleep(SHOW_MS);
         pthread_mutex_lock(&end_game_mut);
        if (end_game >= 1)
        {   
            flag --;
        }
        pthread_mutex_unlock(&end_game_mut);
         pthread_mutex_lock(&quitter);
        if (quit != 0)
        {   
            flag=0;
        }
        pthread_mutex_unlock(&quitter);
       
        for(int i=0;i<num_regions;i++){
            pthread_mutex_lock(&board_mut[i]);
        }
        for (int i = 0; i < num_regions; i++)
        {   if(board[i].owner =='A'){
            pointsa++;
        }
        if(board[i].owner=='B'){
            pointsb++;
        }
            printf("%d [%c] : ", i, board[i].owner);
            for (int y = 0; y < board[i].num_neighbors; y++)
            {
                printf("%d;", board[i].neighbors[y]);
            }
            printf("\n");
        }
        printf("Points of king 'A': %d\nPoints of king 'B': %d\n", pointsa, pointsb);
        for(int i=0;i<num_regions;i++){
            pthread_mutex_unlock(&board_mut[i]);
        }
        
    }
    
    pthread_join(player1, NULL);
    pthread_join(player2, NULL);
    free(board_mut);
    free(board);
}
