using System;
using System.Collections.Generic;
using System.Linq;

namespace GitRepositoryLab
{
    // Commit class
    public class Commit
    {
        public string Hash { get; set; }
        public string Message { get; set; }
        public string Author { get; set; }
        public List<string> Parents { get; set; }
        public HashSet<string> Files { get; set; }
        public int LinesAdded { get; set; }
        public int LinesRemoved { get; set; }

        public Commit(string hash, string message, string author, List<string> parents, HashSet<string> files)
        {
            Hash = hash;
            Message = message;
            Author = author;
            Parents = parents ?? new List<string>();
            Files = files ?? new HashSet<string>();
        }

        public int TotalChanges => LinesAdded + LinesRemoved;
    }

    // Branch class
    public class Branch
    {
        public string Name { get; set; }
        public string CommitHash { get; set; }

        public Branch(string name, string commitHash)
        {
            Name = name;
            CommitHash = commitHash;
        }
    }

    // BaseRef - base class for references
    public abstract class BaseRef
    {
        public abstract string GetCommitHash(Repository repo);
    }

    public class HashRef : BaseRef
    {
        public string Hash { get; set; }
        
        public HashRef(string hash)
        {
            Hash = hash;
        }

        public override string GetCommitHash(Repository repo)
        {
            return Hash;
        }
    }

    public class BranchRef : BaseRef
    {
        public string BranchName { get; set; }

        public BranchRef(string branchName)
        {
            BranchName = branchName;
        }

        public override string GetCommitHash(Repository repo)
        {
            if (repo.Branches.TryGetValue(BranchName, out var branch))
            {
                return branch.CommitHash;
            }
            throw new Exception($"Branch '{BranchName}' not found");
        }
    }

    public class HeadRef : BaseRef
    {
        public override string GetCommitHash(Repository repo)
        {
            if (repo.Head == null)
                throw new Exception("HEAD is not set");
            return repo.Head;
        }
    }

    // Repository class
    public class Repository
    {
        public Dictionary<string, Commit> Objects { get; set; }
        public Dictionary<string, Branch> Branches { get; set; }
        public string Head { get; set; }
        public HashSet<string> Authors { get; set; }

        public Repository()
        {
            Objects = new Dictionary<string, Commit>();
            Branches = new Dictionary<string, Branch>();
            Authors = new HashSet<string>();
        }

        // TryGetCommit - 1 pkt
        public Commit TryGetCommit(string hash)
        {
            Objects.TryGetValue(hash, out var commit);
            return commit;
        }

        // GetCommitOrThrow - 1 pkt
        public Commit GetCommitOrThrow(string hash)
        {
            var commit = TryGetCommit(hash);
            if (commit == null)
            {
                throw new Exception($"Commit '{hash}' not found");
            }
            return commit;
        }

        // TraverseBranchByFirstParent - 1 pkt
        public IEnumerable<Commit> TraverseBranchByFirstParent(string startHash)
        {
            var currentHash = startHash;

            while (!string.IsNullOrEmpty(currentHash))
            {
                var commit = TryGetCommit(currentHash);
                if (commit == null)
                    yield break;

                yield return commit;

                // Follow first parent only
                if (commit.Parents.Any())
                {
                    currentHash = commit.Parents[0];
                }
                else
                {
                    yield break;
                }
            }
        }
    }

    // Extension methods for modifiers
    public static class RepositoryExtensions
    {
        // ParentModifier - 0.5 pkt (HEAD^ or HEAD~1)
        public static Commit ParentModifier(this Repository repo, BaseRef baseRef)
        {
            var commitHash = baseRef.GetCommitHash(repo);
            var commit = repo.TryGetCommit(commitHash);
            
            if (commit == null || !commit.Parents.Any())
                return null;

            return repo.TryGetCommit(commit.Parents[0]);
        }

        // AncestorModifier - 0.5 pkt (HEAD~N)
        public static Commit AncestorModifier(this Repository repo, BaseRef baseRef, int generations)
        {
            var commitHash = baseRef.GetCommitHash(repo);
            var current = repo.TryGetCommit(commitHash);

            for (int i = 0; i < generations; i++)
            {
                if (current == null || !current.Parents.Any())
                    return null;
                
                current = repo.TryGetCommit(current.Parents[0]);
            }

            return current;
        }

        // TraverseByRevision - 1.5 pkt
        public static IEnumerable<Commit> TraverseByRevision(this Repository repo, string pattern)
        {
            BaseRef baseRef;
            int ancestorCount = 0;
            int parentIndex = 0;

            // Parse pattern
            if (pattern == "HEAD")
            {
                baseRef = new HeadRef();
            }
            else if (pattern.StartsWith("HEAD~"))
            {
                baseRef = new HeadRef();
                ancestorCount = int.Parse(pattern.Substring(5));
            }
            else if (pattern.StartsWith("HEAD^"))
            {
                baseRef = new HeadRef();
                if (pattern.Length > 5)
                {
                    parentIndex = int.Parse(pattern.Substring(5)) - 1;
                }
                else
                {
                    ancestorCount = 1; // HEAD^ = HEAD~1
                }
            }
            else if (repo.Branches.ContainsKey(pattern))
            {
                baseRef = new BranchRef(pattern);
            }
            else
            {
                baseRef = new HashRef(pattern);
            }

            // Apply modifiers
            Commit commit;
            if (ancestorCount > 0)
            {
                commit = repo.AncestorModifier(baseRef, ancestorCount);
            }
            else if (parentIndex > 0)
            {
                var commitHash = baseRef.GetCommitHash(repo);
                var baseCommit = repo.TryGetCommit(commitHash);
                if (baseCommit != null && baseCommit.Parents.Count > parentIndex)
                {
                    commit = repo.TryGetCommit(baseCommit.Parents[parentIndex]);
                }
                else
                {
                    commit = null;
                }
            }
            else
            {
                var commitHash = baseRef.GetCommitHash(repo);
                commit = repo.TryGetCommit(commitHash);
            }

            if (commit != null)
            {
                yield return commit;
            }
        }
    }

    // Repository Queries - each 1.5 pkt
    public class RepositoryQueries
    {
        private Repository _repo;

        public RepositoryQueries(Repository repo)
        {
            _repo = repo;
        }

        // Query 1: Który commit zawierał najwięcej zmienionych linii (dodania + usunięcia)
        public Commit CommitWithMostChanges()
        {
            return _repo.Objects.Values
                .OrderByDescending(c => c.TotalChanges)
                .FirstOrDefault();
        }

        // Query 2: Ile plików średnio zmieniają poszczególni autorzy na commit?
        public Dictionary<string, double> AverageFilesPerAuthor()
        {
            return _repo.Objects.Values
                .GroupBy(c => c.Author)
                .ToDictionary(
                    g => g.Key,
                    g => g.Average(c => c.Files.Count)
                );
        }

        // Query 3: Kto jest autorem największej liczby commitów?
        public string AuthorWithMostCommits()
        {
            return _repo.Objects.Values
                .GroupBy(c => c.Author)
                .OrderByDescending(g => g.Count())
                .Select(g => g.Key)
                .FirstOrDefault();
        }

        // Query 4: Który commit był pierwszym, który dodawał do repozytorium dany plik?
        public Commit FirstCommitWithFile(string filename)
        {
            // This assumes commits are ordered by time
            return _repo.Objects.Values
                .Where(c => c.Files.Contains(filename))
                .OrderBy(c => c.Hash) // In reality, should order by timestamp
                .FirstOrDefault();
        }

        // Query 5: Które pliki mają najwięcej współpracujących ze sobą autorów?
        public Dictionary<string, int> FilesWithMostAuthors()
        {
            var fileAuthors = new Dictionary<string, HashSet<string>>();

            foreach (var commit in _repo.Objects.Values)
            {
                foreach (var file in commit.Files)
                {
                    if (!fileAuthors.ContainsKey(file))
                    {
                        fileAuthors[file] = new HashSet<string>();
                    }
                    fileAuthors[file].Add(commit.Author);
                }
            }

            return fileAuthors
                .ToDictionary(kv => kv.Key, kv => kv.Value.Count)
                .OrderByDescending(kv => kv.Value)
                .ToDictionary(kv => kv.Key, kv => kv.Value);
        }

        // Bonus: All queries combined in XML output format
        public string GetAllQueriesAsXml()
        {
            var results = new
            {
                MostChanges = CommitWithMostChanges()?.Hash,
                AvgFilesPerAuthor = AverageFilesPerAuthor(),
                TopAuthor = AuthorWithMostCommits(),
                FilesAuthors = FilesWithMostAuthors()
            };

            // Convert to XML or return as needed
            return Newtonsoft.Json.JsonConvert.SerializeObject(results, Newtonsoft.Json.Formatting.Indented);
        }
    }

    // Example usage
    class Program
    {
        static void Main(string[] args)
        {
            // Create sample repository
            var repo = new Repository();

            // Add commits
            var c1 = new Commit("a1b2c3", "Initial commit", "Alice", 
                new List<string>(), 
                new HashSet<string> { "file1.txt", "file2.txt" })
            {
                LinesAdded = 100,
                LinesRemoved = 0
            };

            var c2 = new Commit("d4e5f6", "Add feature", "Bob", 
                new List<string> { "a1b2c3" }, 
                new HashSet<string> { "file3.txt" })
            {
                LinesAdded = 50,
                LinesRemoved = 10
            };

            var c3 = new Commit("g7h8i9", "Fix bug", "Alice", 
                new List<string> { "d4e5f6" }, 
                new HashSet<string> { "file1.txt" })
            {
                LinesAdded = 20,
                LinesRemoved = 15
            };

            repo.Objects["a1b2c3"] = c1;
            repo.Objects["d4e5f6"] = c2;
            repo.Objects["g7h8i9"] = c3;

            repo.Branches["main"] = new Branch("main", "g7h8i9");
            repo.Head = "g7h8i9";
            repo.Authors.Add("Alice");
            repo.Authors.Add("Bob");

            // Test TryGetCommit
            var commit = repo.TryGetCommit("a1b2c3");
            Console.WriteLine($"Found commit: {commit?.Message ?? "None"}");

            // Test GetCommitOrThrow
            try
            {
                var commit2 = repo.GetCommitOrThrow("invalid");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Exception: {ex.Message}");
            }

            // Test TraverseBranchByFirstParent
            Console.WriteLine("\nTraversing from HEAD:");
            foreach (var c in repo.TraverseBranchByFirstParent("g7h8i9"))
            {
                Console.WriteLine($"  {c.Hash.Substring(0, 6)}: {c.Message}");
            }

            // Test ParentModifier
            var parent = repo.ParentModifier(new HeadRef());
            Console.WriteLine($"\nParent of HEAD: {parent?.Message ?? "None"}");

            // Test AncestorModifier
            var ancestor = repo.AncestorModifier(new HeadRef(), 2);
            Console.WriteLine($"HEAD~2: {ancestor?.Message ?? "None"}");

            // Test TraverseByRevision
            Console.WriteLine("\nTraverse by revision 'HEAD~1':");
            foreach (var c in repo.TraverseByRevision("HEAD~1"))
            {
                Console.WriteLine($"  {c.Hash.Substring(0, 6)}: {c.Message}");
            }

            // Test Queries
            var queries = new RepositoryQueries(repo);

            Console.WriteLine($"\nCommit with most changes: {queries.CommitWithMostChanges()?.Hash}");
            Console.WriteLine($"Author with most commits: {queries.AuthorWithMostCommits()}");
            
            Console.WriteLine("\nAverage files per author:");
            foreach (var kv in queries.AverageFilesPerAuthor())
            {
                Console.WriteLine($"  {kv.Key}: {kv.Value:F2}");
            }

            Console.WriteLine("\nFiles with most authors:");
            foreach (var kv in queries.FilesWithMostAuthors())
            {
                Console.WriteLine($"  {kv.Key}: {kv.Value} authors");
            }

            Console.ReadKey();
        }
    }
}
