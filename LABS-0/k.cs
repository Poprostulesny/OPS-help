using System;
using System.Collections.Generic;
using System.Linq;

namespace Lab2
{
    static class RepositoryExtensions
    {
        public static bool TryGetCommit(this Repository repo, string hash, out Commit commit)
        {
            if (repo.Objects.TryGetValue(hash, out var obj))
            {
                commit = obj;
                return true;
            }
            commit = null;
            return false;
        }

        public static Commit GetCommitOrThrow(this Repository repo, string hash)
        {
            if (!repo.Objects.TryGetValue(hash, out var commit))
                throw new Exception($"Commit '{hash}' not found");
            return commit;
        }

        public static IEnumerable<Commit> TraverseBranchByFirstParent(this Repository repo, string startHash)
        {
            var current = repo.GetCommitOrThrow(startHash);
            while (current != null)
            {
                yield return current;
                if (current.ParentHashes.Count == 0)
                    yield break;

                var parentHash = current.ParentHashes.First();
                if (!repo.Objects.TryGetValue(parentHash, out current))
                    yield break;
            }
        }

        /// <summary>
        /// Główna metoda odwzorowująca działanie Git Revisions (HEAD, HEAD~2, HEAD^1, itp.)
        /// </summary>
        public static IEnumerable<Commit> TraverseByRevision(this Repository repo, string pattern)
        {
            // 1️⃣ Rozbij pattern (np. HEAD~2^1~3)
            var revision = Revision.Parse(pattern);

            // 2️⃣ Pobierz bazowy commit na podstawie BaseRef
            string baseHash = null;

            if (revision.BaseRef == "HEAD")
            {
                baseHash = repo.HEAD;
            }
            else if (repo.Branches.ContainsKey(revision.BaseRef))
            {
                baseHash = repo.Branches[revision.BaseRef];
            }
            else if (repo.Objects.ContainsKey(revision.BaseRef))
            {
                baseHash = revision.BaseRef;
            }
            else
            {
                throw new Exception("Base reference not found");
            }

            var commits = new List<Commit> { repo.GetCommitOrThrow(baseHash) };

            // 3️⃣ Zastosuj po kolei modyfikatory (np. ~2, ^1)
            foreach (var modifier in revision.Modifiers)
            {
                commits = modifier.Apply(repo, commits).ToList();
            }

            // 4️⃣ Zwróć wynikową sekwencję commitów
            return commits;
        }
    }

    // ============================================
    // PARSER REWIZJI (HEAD~2^1~3 → BaseRef + Modifiers)
    // ============================================

    class Revision
    {
        public string BaseRef { get; set; }
        public List<RevisionModifier> Modifiers { get; } = new();

        /// <summary>
        /// Parsuje zapis typu HEAD~3^2~1 do struktury Revision
        /// </summary>
        public static Revision Parse(string pattern)
        {
            var result = new Revision();

            // Wydziel nazwę bazową (np. HEAD)
            int i = 0;
            while (i < pattern.Length && pattern[i] != '~' && pattern[i] != '^')
                i++;

            result.BaseRef = pattern[..i];

            // Parsuj kolejne modyfikatory
            while (i < pattern.Length)
            {
                char symbol = pattern[i++];
                int value = 1; // domyślna wartość

                int start = i;
                while (i < pattern.Length && char.IsDigit(pattern[i]))
                    i++;

                if (i > start)
                    value = int.Parse(pattern[start..i]);

                if (symbol == '~')
                    result.Modifiers.Add(new AncestorModifier { N = value });
                else if (symbol == '^')
                    result.Modifiers.Add(new ParentModifier { Index = value - 1 });
            }

            return result;
        }
    }

    // ============================================
    // MODYFIKATORY REWIZJI
    // ============================================

    abstract class RevisionModifier
    {
        public abstract IEnumerable<Commit> Apply(Repository repo, IEnumerable<Commit> commits);
    }

    class ParentModifier : RevisionModifier
    {
        public int Index { get; set; }

        public override IEnumerable<Commit> Apply(Repository repo, IEnumerable<Commit> commits)
        {
            foreach (var commit in commits)
            {
                if (commit.ParentHashes.Count <= Index)
                    throw new Exception($"Commit '{commit.Hash}' does not have parent #{Index}");
                yield return repo.GetCommitOrThrow(commit.ParentHashes[Index]);
            }
        }
    }

    class AncestorModifier : RevisionModifier
    {
        public int N { get; set; }

        public override IEnumerable<Commit> Apply(Repository repo, IEnumerable<Commit> commits)
        {
            foreach (var commit in commits)
            {
                var current = commit;
                for (int i = 0; i < N; i++)
                {
                    if (current.ParentHashes.Count == 0)
                        throw new Exception($"Commit '{current.Hash}' has no parent");
                    current = repo.GetCommitOrThrow(current.ParentHashes[0]);
                }
                yield return current;
            }
        }
    }
}
