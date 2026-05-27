# Data generator do L8: Mages' Chess

To narzedzie jest recznym generatorem danych testowych. Nie uruchamia serwera i nie ocenia, czy wynik jest poprawny. Wysyla tylko przygotowane datagramy UDP, a Ty patrzysz na stdout serwera i ewentualne odpowiedzi UDP.

## Start

Terminal 1:

```bash
cd "UDP/mages chess"
make
./sop-mag 12345
```

Terminal 2:

```bash
cd "UDP/mages chess"
python3 data-generator/mages_data.py 12345 --stage stage1
```

## Scenariusze

`stage1`

- login `Merlin`,
- cast `Fireball` na `3,4`,
- quit,
- login `Eleonora` jako czwarta wiadomosc.

`validation`

- datagram zlego rozmiaru,
- zly typ wiadomosci,
- zly numer zaklecia,
- zle wspolrzedne,
- poprawny login na koncu.

`stage2`

- loguje dwoch graczy,
- wysyla serie castow do kolejki.

Liczbe castow ustawiasz przez:

```bash
python3 data-generator/mages_data.py 12345 --stage stage2 --count 30
```

`stage3`

- cast przed loginem,
- login dwoch graczy z roznych portow UDP,
- cast od niezalogowanego klienta,
- poprawny cast od gracza,
- quit drugiego gracza.

`stage4`

- login dwoch graczy,
- pierwszy gracz rzuca `Summon Elemental` na `2,2`,
- drugi gracz rzuca `Divination` na `2,2`,
- pierwszy gracz rzuca `Fireball` na `2,2`.

Do podgladania odpowiedzi UDP uzyj:

```bash
python3 data-generator/mages_data.py 12345 --stage stage4 --listen 3
```

## Pojedyncze wiadomosci

Login:

```bash
python3 data-generator/mages_data.py 12345 --stage custom --message login --name Archibald
```

Cast:

```bash
python3 data-generator/mages_data.py 12345 --stage custom --message cast --spell 2 --x 3 --y 4
```

Quit:

```bash
python3 data-generator/mages_data.py 12345 --stage custom --message quit
```

Zle dane:

```bash
python3 data-generator/mages_data.py 12345 --stage custom --message bad-size
python3 data-generator/mages_data.py 12345 --stage custom --message bad-type
```

## Przydatne opcje

`--dry-run` wypisuje bajty, ale nic nie wysyla:

```bash
python3 data-generator/mages_data.py 12345 --stage stage3 --dry-run
```

Domyslnie generator wysyla liczby `uint16_t` w network byte order, czyli big endian.

Mozesz podac to jawnie tak:

```bash
python3 data-generator/mages_data.py 12345 --stage stage4 --endian big --listen 3
```

Jesli chcesz wysylac liczby w kolejnosci lokalnej typowej dla x86_64, uzyj:

```bash
python3 data-generator/mages_data.py 12345 --stage stage4 --endian little --listen 3
```

Domyslnie generator czeka `0.30s` po kazdym datagramie. To pomaga utrzymac czytelny stdout serwera, gdy familiars/judge wypisuja z roznych watkow.

`--delay` pozwala zmienic te przerwe:

```bash
python3 data-generator/mages_data.py 12345 --stage stage2 --delay 0.5
```

## Format pakietow

Generator wysyla pakiety zgodne z trescia zadania:

- 16 bajtow dla poprawnych wiadomosci,
- bajt `0`: typ `l`, `c` albo `q`,
- bajt `1`: padding `\0`,
- bajty `2..15`: cialo wiadomosci dopelnione zerami.

Dla castow cialo zawiera trzy wartosci `uint16_t`: `spell`, `x`, `y`.
