# Created by Spokboud
from pathlib import Path
import os
import sys
import shutil
import time
import random
import tempfile
import requests
import git
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass, field
from typing import List, Optional, Dict, Any, Union, Tuple
import backoff
from tqdm import tqdm
import logging
from datetime import datetime
import threading
lock = threading.Lock()

# Configuration
DEBUG = False  # Set to False for production mode
DRY_RUN = False  # Set to True to only list repositories without making changes
# STAFF_TEST_REPO = "course-tp1-h25-forked-staff-test"  # Staff test repository name
STAFF_TEST_REPO = "hiver25-tp2-os-staff-test"  # Staff test repository name

@dataclass
class FileInfo:
    source_path: str
    target_dir: str


@dataclass
class RepoInfo:
    name: str
    clone_url: str
    default_branch: str
    full_name: str
    created_at: Optional[str] = None
    updated_at: Optional[str] = None
    fork: bool = False
    parent_name: Optional[str] = None

    @classmethod
    def from_api_response(cls, data: Dict[str, Any]) -> 'RepoInfo':
        return cls(
            name=data['name'],
            clone_url=data['clone_url'],
            default_branch=data['default_branch'],
            full_name=data['full_name'],
            created_at=data.get('created_at'),
            updated_at=data.get('updated_at'),
            fork=data.get('fork', False),
            parent_name=data.get('parent', {}).get('full_name')
        )


@dataclass
class ProcessingResult:
    repo_name: str
    success: bool
    error: Optional[str]
    start_time: float
    end_time: float
    duration: float
    commit_hash: Optional[str] = None
    retry_count: int = 0
    files_processed: List[str] = field(default_factory=list)


@dataclass
class ProcessingStats:
    total_repos: int = 0
    successful: int = 0
    failed: int = 0
    total_duration: float = 0.0
    retries: int = 0
    errors: Dict[str, int] = field(default_factory=dict)


class GitHubClassroom:
    def __init__(
            self,
            token: Optional[str] = None,
            org: str = 'IFT2245',
            assignment: str = 'hiver25-tp2-os',
            debug: bool = DEBUG,
            max_workers: int = 4,
            max_retries: int = 3,
            log_file: Optional[str] = None
    ):
        self.token = token or os.getenv('GITHUB_TOKEN')
        if not self.token:
            raise ValueError("GitHub token required")
        self.org = org
        self.assignment = assignment
        self.debug = debug
        self.max_workers = max_workers
        self.max_retries = max_retries
        self.headers = {'Authorization': f'token {self.token}', 'Accept': 'application/vnd.github.v3+json'}
        self.temp_dir = Path(tempfile.mkdtemp())
        self.stats = ProcessingStats()

        # Setup logging
        self.logger = logging.getLogger(__name__)
        self.logger.setLevel(logging.DEBUG if debug else logging.INFO)
        formatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s')

        # Console handler
        console_handler = logging.StreamHandler()
        console_handler.setFormatter(formatter)
        self.logger.addHandler(console_handler)

        # File handler if specified
        if log_file:
            file_handler = logging.FileHandler(log_file)
            file_handler.setFormatter(formatter)
            self.logger.addHandler(file_handler)

    @backoff.on_exception(backoff.expo,
                          requests.exceptions.RequestException,
                          max_tries=3,
                          on_backoff=lambda details: print(f"Retrying API call... (attempt {details['tries']})"))
    def _get_repos(self) -> List[RepoInfo]:
        repos = []
        page = 1
        self.logger.info("Fetching repositories...")

        while True:
            try:
                resp = requests.get(
                    f'https://api.github.com/orgs/{self.org}/repos',
                    headers=self.headers,
                    params={'page': page, 'per_page': 100, 'type': 'all'},
                    timeout=10
                )
                resp.raise_for_status()
                data = resp.json()

                if not data:
                    break

                for r in data:
                    n = r['name'].lower()
                    if self.debug and STAFF_TEST_REPO in n:
                        return [RepoInfo.from_api_response(r)]
                    if n.startswith(f"{self.assignment}-") or STAFF_TEST_REPO in n:
                        repos.append(RepoInfo.from_api_response(r))

                page += 1

            except requests.exceptions.RequestException as e:
                self.logger.error(f"Error fetching repositories: {str(e)}")
                raise

        if not repos and not self.debug:
            self.logger.warning("No repositories found.")

        return repos

    def _process_repo(self, repo: RepoInfo, files: List[FileInfo], commit_msg: str) -> ProcessingResult:
        start_time = time.time()
        result = ProcessingResult(repo.name, False, None, start_time, 0, 0)
        repo_path = self.temp_dir / repo.name

        for attempt in range(self.max_retries):
            try:
                # Clone and checkout
                g = git.Repo.clone_from(repo.clone_url, repo_path)
                g.git.checkout(repo.default_branch)

                files_processed = []
                for file_info in files:
                    # Ensure target directory path is clean and relative
                    target_dir_path = Path(file_info.target_dir)
                    if target_dir_path.is_absolute():
                        target_dir_path = target_dir_path.relative_to(target_dir_path.root)

                    # Create target directory in repo
                    target_path = repo_path / target_dir_path
                    target_path.mkdir(parents=True, exist_ok=True)

                    # Get source file name and ensure it's just the filename
                    source_file = Path(file_info.source_path)
                    dest_file = target_path / source_file.name

                    # Copy file to target directory
                    shutil.copy2(file_info.source_path, dest_file)

                    # Convert to git-style path with forward slashes
                    rel_path = str(target_dir_path / source_file.name).replace(os.sep, '/')
                    files_processed.append(rel_path)
                    result.files_processed.append(rel_path)

                # Check if any files were actually changed
                if not g.is_dirty(untracked_files=True):
                    self.logger.info(f"✅ No changes needed in {repo.name} - files are identical")
                    result.success = True
                    result.retry_count = attempt
                    break

                # Add and commit all changes at once
                g.index.add(files_processed)
                commit = g.index.commit(f"{commit_msg}, !33+")

                # Push changes
                with lock:
                    # time.sleep(random.uniform(1.0, 5.0))
                    g.remote().push()
                result.commit_hash = commit.hexsha
                result.success = True
                result.retry_count = attempt
                self.logger.info(f"✅ Processed {repo.name} with {len(files_processed)} files")
                break

            except Exception as e:
                result.error = str(e)
                self.logger.error(f"❌ Error in {repo.name} (attempt {attempt + 1}/{self.max_retries}): {e}")
                if attempt < self.max_retries - 1:
                    time.sleep(2 ** attempt)  # Exponential backoff
                shutil.rmtree(repo_path, ignore_errors=True)

            finally:
                if attempt == self.max_retries - 1:
                    result.end_time = time.time()
                    result.duration = result.end_time - start_time
                    shutil.rmtree(repo_path, ignore_errors=True)

        return result

    def _write_repos_to_files(self, kept: List[RepoInfo], excluded: List[RepoInfo]) -> None:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")

        # Write excluded repos
        excluded_file = f"excluded_repos_{timestamp}.txt"
        with open(excluded_file, "w") as f:
            f.write("# Excluded Repositories\n")
            f.write(f"# Generated: {datetime.now().isoformat()}\n")
            f.write("# Format: full_name,created_at,updated_at,parent_name\n\n")
            for x in excluded:
                f.write(f"{x.full_name},{x.created_at},{x.updated_at},{x.parent_name or 'N/A'}\n")

        # Write included repos
        included_file = f"included_repos_{timestamp}.txt"
        with open(included_file, "w") as f:
            f.write("# Included Repositories\n")
            f.write(f"# Generated: {datetime.now().isoformat()}\n")
            f.write("# Format: full_name,created_at,updated_at,parent_name\n\n")
            for x in kept:
                f.write(f"{x.full_name},{x.created_at},{x.updated_at},{x.parent_name or 'N/A'}\n")

        self.logger.info(f"\nRepository lists have been written to '{included_file}' and '{excluded_file}'")

    def _update_stats(self, results: List[ProcessingResult]) -> None:
        self.stats.total_repos = len(results)
        self.stats.successful = sum(1 for r in results if r.success)
        self.stats.failed = len(results) - self.stats.successful
        self.stats.total_duration = sum(r.duration for r in results)
        self.stats.retries = sum(r.retry_count for r in results)

        # Collect error types
        for r in results:
            if r.error:
                error_type = r.error.split(':')[0]
                self.stats.errors[error_type] = self.stats.errors.get(error_type, 0) + 1

    def _print_stats(self) -> None:
        self.logger.info("\nProcessing Statistics:")
        self.logger.info(f"Total Repositories: {self.stats.total_repos}")
        self.logger.info(f"Successful: {self.stats.successful}")
        self.logger.info(f"Failed: {self.stats.failed}")
        self.logger.info(f"Total Duration: {self.stats.total_duration:.2f} seconds")
        self.logger.info(f"Total Retries: {self.stats.retries}")

        if self.stats.errors:
            self.logger.info("\nError Summary:")
            for error_type, count in self.stats.errors.items():
                self.logger.info(f"  {error_type}: {count}")

    def run(
            self,
            files: Optional[List[FileInfo]] = None,
            commit_msg: str = "Add files",
            dry_run: bool = DRY_RUN,
    ) -> Union[Tuple[List[ProcessingResult], List[RepoInfo]], List[RepoInfo]]:
        """
        Run the GitHub classroom file deployment process.

        Args:
            files: List of FileInfo objects containing source paths and target directories
            commit_msg: Commit message for the changes
            dry_run: If True, only list repositories without making changes

        Returns:
            If dry_run is True: List of RepoInfo objects
            If dry_run is False: Tuple of (List of ProcessingResults, List of RepoInfo objects)
        """
        self.logger.info(f"{'🔍 DEBUG' if self.debug else '🚀 PROD'}: Starting...")

        # Validate inputs
        if not dry_run:
            if not files:
                raise ValueError("files list is required when not in dry_run mode")

            # Validate all files and directories
            for file_info in files:
                file_path = Path(file_info.source_path)
                if not file_path.is_file():
                    raise FileNotFoundError(f"File {file_path} not found")

                if not file_info.target_dir:
                    raise ValueError("target_dir cannot be empty")

                # Ensure target_dir doesn't try to escape repository root
                if '..' in Path(file_info.target_dir).parts:
                    raise ValueError("target_dir cannot contain parent directory references (..)")

        repos = self._get_repos()
        if not repos:
            return [] if dry_run else ([], [])

        self.logger.info(f"Found {len(repos)} {'staff test repo' if self.debug else 'repos'}")

        # Filter repos based on parent
        self.logger.info("Filtering repos based on parent name...")
        patterns = ["hiver25-tp2-os"]
        kept, excluded = [], []

        with ThreadPoolExecutor(max_workers=self.max_workers) as ex:
            futures = [ex.submit(fetch_parent_info, r, self.token, patterns) for r in repos]
            for r, fut in zip(repos, tqdm(as_completed(futures), total=len(futures), desc="Filtering")):
                try:
                    if fut.result():
                        kept.append(r)
                    else:
                        excluded.append(r)
                except Exception as e:
                    self.logger.error(f"Error retrieving parent info for {r.full_name}: {e}")
                    excluded.append(r)

        self.logger.info(f"\nKept {len(kept)} repos after filtering.")
        self.logger.info(f"Excluded {len(excluded)} repos after filtering.")

        # Write repos to files
        self._write_repos_to_files(kept, excluded)

        if dry_run:
            return kept

        # Confirmation step
        while True:
            try:
                choice = input("\nPress 1 to proceed with processing, 2 to cancel: ")
                if choice == "1":
                    break
                elif choice == "2":
                    self.logger.info("Operation cancelled by user")
                    return [], kept
                else:
                    print("Invalid input. Please press 1 to proceed or 2 to cancel.")
            except KeyboardInterrupt:
                self.logger.info("\nOperation cancelled by user")
                return [], kept

        # Process repos
        results = []

        # Verify each repository before processing
        self.logger.info("Verifying repository access...")
        accessible_repos = []
        for repo in tqdm(kept, desc="Verifying repositories"):
            try:
                headers = {'Authorization': f'token {self.token}'}
                r = requests.get(f"https://api.github.com/repos/{repo.full_name}", headers=headers)
                r.raise_for_status()

                # Check push access
                if r.json().get('permissions', {}).get('push', False):
                    accessible_repos.append(repo)
                else:
                    self.logger.warning(f"No push access to {repo.full_name}, skipping")
                    results.append(ProcessingResult(
                        repo_name=repo.name,
                        success=False,
                        error="No push access",
                        start_time=time.time(),
                        end_time=time.time(),
                        duration=0
                    ))
            except Exception as e:
                self.logger.error(f"Error verifying {repo.full_name}: {str(e)}")
                results.append(ProcessingResult(
                    repo_name=repo.name,
                    success=False,
                    error=f"Verification failed: {str(e)}",
                    start_time=time.time(),
                    end_time=time.time(),
                    duration=0
                ))

        if not accessible_repos:
            self.logger.error("No accessible repositories found")
            return results, kept

        self.logger.info(f"Found {len(accessible_repos)} accessible repositories")

        # Process repositories
        with ThreadPoolExecutor(max_workers=self.max_workers) as executor:
            futures = {
                executor.submit(self._process_repo, r, files, commit_msg): r
                for r in accessible_repos
            }
            for f in tqdm(as_completed(futures), total=len(futures), desc="Processing"):
                results.append(f.result())

        # Update and print statistics
        self._update_stats(results)
        self._print_stats()

        # Cleanup
        shutil.rmtree(self.temp_dir, ignore_errors=True)
        return results, kept


@backoff.on_exception(backoff.expo,
                      requests.exceptions.RequestException,
                      max_tries=3,
                      on_backoff=lambda details: print(f"Retrying API call... (attempt {details['tries']})"))
def fetch_parent_info(repo: RepoInfo, token: str, patterns: List[str]) -> bool:
    headers = {'Authorization': f'token {token}', 'Accept': 'application/vnd.github.v3+json'}
    r = requests.get(f"https://api.github.com/repos/{repo.full_name}", headers=headers, timeout=10)
    r.raise_for_status()
    data = r.json()
    p = data.get('parent', {}).get('full_name', '')
    return data.get('fork') and any(x.lower() in p.lower() for x in patterns)


def main():
    try:
        # Define the files to process
        files_to_process = [
            # FileInfo(source_path="dummy.txt", target_dir="test"),
            #

            FileInfo(source_path="README.md", target_dir="."),

            # FileInfo(source_path=".github/workflows/classroom.yml", target_dir=".github/workflows"),
            # FileInfo(source_path="test-results-edge/autograder.py", target_dir="test"),
            # FileInfo(source_path="test/edge-test.h", target_dir="test"),
            # FileInfo(source_path="test/edge-test.c", target_dir="test"),
            # FileInfo(source_path="src/tokenizer.c", target_dir="src"),
            # FileInfo(source_path="src/shell.h", target_dir="src"),
            # FileInfo(source_path="src/CMakeLists.txt", target_dir="src")

            # FileInfo(source_path="names.txt", target_dir=".")
            # Adding more files as needed
        ]

        # Initialize with logging
        c = GitHubClassroom(
            debug=DEBUG,  # Uses the global DEBUG setting
            max_workers=4,
            max_retries=3,
            log_file=f"github_classroom_{datetime.now().strftime('%Y%m%d_%H%M%S')}.log"
        )

        if DRY_RUN:
            repos = c.run(dry_run=True)
            if not repos:
                return 0
        else:
            results, repos = c.run(
                files=files_to_process,
                commit_msg="context by leet",
                dry_run=False
            )
            if not repos:
                return 0
        return 0

    except KeyboardInterrupt:
        print("\n⚠️ Cancelled")
        return 130
    except Exception as e:
        print(f"❌ Fatal: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())