#!/usr/bin/env python3
# Copyright (C) 2025 Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

"""
License header management tool for source code files.

This script provides functionality to add, update, remove, and check license headers
in source code files across multiple programming languages.
"""

import argparse
import logging
import os
import re
import shutil
import sys
import tempfile
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class LicenseStatus(Enum):
    """Enum representing the license status of a file."""
    HAS_VALID_LICENSE = "has_valid_license"
    HAS_OUTDATED_LICENSE = "has_outdated_license"
    NO_LICENSE = "no_license"
    ERROR = "error"


@dataclass
class FileProcessingResult:
    """Result of processing a single file."""
    filepath: Path
    status: LicenseStatus
    message: str
    modified: bool = False


@dataclass
class LicenseConfig:
    """Config for license processing."""
    license_file: Path
    comment_styles: Dict[str, str]
    file_patterns: List[str]
    exclude_patterns: List[str]
    dry_run: bool = False
    backup: bool = True
    verbose: bool = False


class LicenseManager:
    """Manages license headers in source code files."""
    
    # Default file patterns to process
    DEFAULT_FILE_PATTERNS = [
        "*.py", "*.cpp", "*.h", "*.hpp", "*.c", "*.cc", "*.go", 
        "*.sh", "*.js", "*.ts", "*.java", "*.rs", "*.rb",
        "Makefile", "*Dockerfile*"
    ]
    
    # Default directories/files to exclude
    DEFAULT_EXCLUDE_PATTERNS = [
        "*.git*", "*build*", "*dist*", "*node_modules*", 
        "*__pycache__*", "*.egg-info*", "*venv*", "*env*"
    ]
    
    # Comment styles for different file types
    DEFAULT_COMMENT_STYLES = {
        ".py": "#",
        ".sh": "#",
        ".conf": "#",
        ".yml": "#",
        ".yaml": "#",
        ".toml": "#",
        ".ini": "#",
        ".cpp": "//",
        ".h": "//", 
        ".hpp": "//",
        ".c": "//",
        ".cc": "//",
        ".cxx": "//",
        ".go": "//",
        ".js": "//",
        ".ts": "//",
        ".java": "//",
        ".rs": "//",
        ".rb": "#",
        ".pl": "#",
        ".r": "#",
        "Dockerfile": "#",
        "Makefile": "#",
        ".mk": "#",
        ".cmake": "#"
    }
    
    def __init__(self, config: LicenseConfig):
        """Initialize the LicenseManager with config."""
        self.config = config
        self._validate_config()
        self._license_content = self._load_license()
        
    def _validate_config(self) -> None:
        """Validate the config."""
        if not self.config.license_file.exists():
            raise FileNotFoundError(f"License file not found: {self.config.license_file}")
        
        if not self.config.license_file.is_file():
            raise ValueError(f"License path is not a file: {self.config.license_file}")
    
    def _load_license(self) -> List[str]:
        """Load and validate the license file content."""
        try:
            with open(self.config.license_file, 'r', encoding='utf-8') as f:
                content = f.read().strip().splitlines()
            
            if not content:
                raise ValueError("License file is empty")
            
            logger.info(f"Loaded license from {self.config.license_file} ({len(content)} lines)")
            return content
            
        except UnicodeDecodeError as e:
            raise ValueError(f"License file contains invalid UTF-8: {e}")
        except Exception as e:
            raise RuntimeError(f"Failed to read license file: {e}")
    
    def _get_comment_style(self, filepath: Path) -> str:
        """Determine the comment style for a file."""
        # Check files without extensions (e.g. Dockerfile, Makefile)
        if filepath.name in self.config.comment_styles:
            return self.config.comment_styles[filepath.name]
        
        # Check by extension
        suffix = filepath.suffix.lower()
        if suffix in self.config.comment_styles:
            return self.config.comment_styles[suffix]
        
        # Unknown filetype, error
        raise ValueError(
            f"Unknown file type for {filepath} (extension: '{suffix}'). "
            f"Please add the appropriate comment style to the config or "
            f"exclude this file type from processing."
        )
    
    def _insert_comments_in_license(self, filepath: Path) -> List[str]:
        """Add comments to the license the proper comment style."""
        comment_style = self._get_comment_style(filepath)
        
        formatted_lines = []
        for line in self._license_content:
            if line.strip():  # Non-empty line
                formatted_lines.append(f"{comment_style} {line}")
            else:  # Empty line
                formatted_lines.append(comment_style)
        
        return formatted_lines
    
    def _has_shebang(self, lines: List[str]) -> bool:
        """Check if the file starts with a shebang to add the license under it."""
        return len(lines) > 0 and lines[0].startswith('#!')
    
    def _find_license_in_file(self, content: str, formatted_license: List[str]) -> Optional[Tuple[int, int]]:
        """Find the lines of the license in a file if one exists. Returns (start_line, end_line) or None."""
        content_lines = content.splitlines()
        license_text = '\n'.join(formatted_license)
        
        if license_text in content:
            for i in range(len(content_lines)):
                potential_end = i + len(formatted_license)
                if potential_end <= len(content_lines):
                    section = '\n'.join(content_lines[i:potential_end])
                    if section == license_text:
                        return (i, potential_end - 1)
        
        return None
    
    def _determine_license_status(self, filepath: Path) -> LicenseStatus:
        """Detect the current license status of a file."""
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                content = f.read()
            
            # Check for any copyright notice
            copyright_patterns = [
                r'Copyright\s*\([Cc]\)',
                r'Copyright\s*©', 
                r'Copyright\s+\d{4}',
                r'Copyright\s+[A-Za-z]+'
            ]
            
            has_any_copyright = any(re.search(pattern, content, re.IGNORECASE) 
                                  for pattern in copyright_patterns)
            
            if not has_any_copyright:
                return LicenseStatus.NO_LICENSE
            
            # Check if it matches the specified license
            formatted_license = self._insert_comments_in_license(filepath)
            license_location = self._find_license_in_file(content, formatted_license)
            
            if license_location is not None:
                return LicenseStatus.HAS_VALID_LICENSE
            else:
                return LicenseStatus.HAS_OUTDATED_LICENSE
                
        except UnicodeDecodeError:
            logger.warning(f"Cannot read {filepath} as UTF-8, skipping")
            return LicenseStatus.ERROR
        except Exception as e:
            logger.error(f"Error reading {filepath}: {e}")
            return LicenseStatus.ERROR
    
    def _backup_file(self, filepath: Path) -> Optional[Path]:
        """Create a backup of the file. Returns backup path or None if backup failed."""
        if not self.config.backup:
            return None
        
        try:
            backup_path = filepath.with_suffix(filepath.suffix + '.backup')
            shutil.copy2(filepath, backup_path)
            logger.debug(f"Created backup: {backup_path}")
            return backup_path
        except Exception as e:
            logger.error(f"Failed to create backup for {filepath}: {e}")
            return None
    
    def _write_file_atomically(self, filepath: Path, content: str) -> bool:
        """Write file content atomically using a temporary file."""
        try:
            # Create temporary file in the same directory to ensure atomic move
            temp_dir = filepath.parent
            with tempfile.NamedTemporaryFile(
                mode='w', 
                dir=temp_dir, 
                delete=False, 
                encoding='utf-8'
            ) as temp_file:
                temp_path = Path(temp_file.name)
                temp_file.write(content)
            
            # Preserve original file permissions
            if filepath.exists():
                stat = filepath.stat()
                temp_path.chmod(stat.st_mode)
            
            shutil.move(str(temp_path), str(filepath))
            return True
            
        except Exception as e:
            logger.error(f"Failed to write {filepath}: {e}")
            # Clean up temp file if it exists
            if 'temp_path' in locals() and temp_path.exists():
                temp_path.unlink()
            return False
    
    def add_license(self, filepath: Path) -> FileProcessingResult:
        """Add license header to a file."""
        status = self._determine_license_status(filepath)
        
        if status == LicenseStatus.HAS_VALID_LICENSE:
            return FileProcessingResult(
                filepath=filepath,
                status=status,
                message="Already has valid license"
            )
        
        if status == LicenseStatus.ERROR:
            return FileProcessingResult(
                filepath=filepath,
                status=status,
                message="Cannot process file due to encoding or read error"
            )
        
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                original_content = f.read()
            
            lines = original_content.splitlines()
            formatted_license = self._insert_comments_in_license(filepath)
            
            # Determine insertion point
            insert_position = 1 if self._has_shebang(lines) else 0
            
            # Insert license
            new_lines = (
                lines[:insert_position] + 
                formatted_license + 
                [''] +  # Empty line after license
                lines[insert_position:]
            )
            
            new_content = '\n'.join(new_lines)
            
            if self.config.dry_run:
                logger.info(f"[DRY RUN] Would add license to {filepath}")
                return FileProcessingResult(
                    filepath=filepath,
                    status=LicenseStatus.NO_LICENSE,
                    message="Would add license (dry run)",
                    modified=True
                )
            
            # Create backup
            backup_path = self._backup_file(filepath)
            
            # Write the file
            if self._write_file_atomically(filepath, new_content):
                return FileProcessingResult(
                    filepath=filepath,
                    status=LicenseStatus.HAS_VALID_LICENSE,
                    message="License added successfully",
                    modified=True
                )
            else:
                return FileProcessingResult(
                    filepath=filepath,
                    status=LicenseStatus.ERROR,
                    message="Failed to write file"
                )
                
        except Exception as e:
            logger.error(f"Error processing {filepath}: {e}")
            return FileProcessingResult(
                filepath=filepath,
                status=LicenseStatus.ERROR,
                message=f"Processing error: {e}"
            )
    
    def remove_license(self, filepath: Path) -> FileProcessingResult:
        """Remove license header from a file."""
        status = self._determine_license_status(filepath)
        
        if status == LicenseStatus.NO_LICENSE:
            return FileProcessingResult(
                filepath=filepath,
                status=status,
                message="No license to remove"
            )
        
        if status == LicenseStatus.ERROR:
            return FileProcessingResult(
                filepath=filepath,
                status=status,
                message="Cannot process file due to encoding or read error"
            )
        
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                content = f.read()
            
            formatted_license = self._insert_comments_in_license(filepath)
            license_location = self._find_license_in_file(content, formatted_license)
            
            if license_location is None:
                return FileProcessingResult(
                    filepath=filepath,
                    status=LicenseStatus.HAS_OUTDATED_LICENSE,
                    message="Has copyright but not our exact license format"
                )
            
            lines = content.splitlines()
            start_line, end_line = license_location
            
            # Remove license and any following empty lines
            new_lines = lines[:start_line] + lines[end_line + 1:]
            
            # Remove leading empty lines that might be left after license removal
            while new_lines and not new_lines[0].strip():
                new_lines.pop(0)
            
            new_content = '\n'.join(new_lines)
            
            if self.config.dry_run:
                logger.info(f"[DRY RUN] Would remove license from {filepath}")
                return FileProcessingResult(
                    filepath=filepath,
                    status=status,
                    message="Would remove license (dry run)",
                    modified=True
                )
            
            # Create backup
            backup_path = self._backup_file(filepath)
            
            # Write the file
            if self._write_file_atomically(filepath, new_content):
                return FileProcessingResult(
                    filepath=filepath,
                    status=LicenseStatus.NO_LICENSE,
                    message="License removed successfully",
                    modified=True
                )
            else:
                return FileProcessingResult(
                    filepath=filepath,
                    status=LicenseStatus.ERROR,
                    message="Failed to write file"
                )
                
        except Exception as e:
            logger.error(f"Error processing {filepath}: {e}")
            return FileProcessingResult(
                filepath=filepath,
                status=LicenseStatus.ERROR,
                message=f"Processing error: {e}"
            )
    
    def update_license(self, filepath: Path) -> FileProcessingResult:
        """Update license header in a file."""
        # Remove existing license first
        remove_result = self.remove_license(filepath)
        
        if remove_result.status == LicenseStatus.ERROR:
            return remove_result
        
        # Add new license
        return self.add_license(filepath)
    
    def check_license(self, filepath: Path) -> FileProcessingResult:
        """Check license status of a file."""
        status = self._determine_license_status(filepath)
        
        messages = {
            LicenseStatus.HAS_VALID_LICENSE: "Has valid license",
            LicenseStatus.HAS_OUTDATED_LICENSE: "Has outdated or different license",
            LicenseStatus.NO_LICENSE: "Missing license header",
            LicenseStatus.ERROR: "Cannot process file"
        }
        
        return FileProcessingResult(
            filepath=filepath,
            status=status,
            message=messages[status]
        )
    
    def find_source_files(self, paths: List[Path]) -> List[Path]:
        """Find all source files matching the configured patterns."""
        source_files = set()
        
        for path in paths:
            if path.is_file():
                source_files.add(path)
            elif path.is_dir():
                for pattern in self.config.file_patterns:
                    source_files.update(path.rglob(pattern))
        
        # Filter out excluded patterns
        filtered_files = []
        for filepath in source_files:
            should_exclude = any(
                filepath.match(pattern) for pattern in self.config.exclude_patterns
            )
            if not should_exclude:
                filtered_files.append(filepath)
        
        return sorted(filtered_files)


def create_config_from_args(args: argparse.Namespace) -> LicenseConfig:
    """Instantiate LicenseConfig from cli."""
    comment_styles = LicenseManager.DEFAULT_COMMENT_STYLES.copy()
    
    return LicenseConfig(
        license_file=Path(args.license),
        comment_styles=comment_styles,
        file_patterns=LicenseManager.DEFAULT_FILE_PATTERNS,
        exclude_patterns=LicenseManager.DEFAULT_EXCLUDE_PATTERNS,
        dry_run=args.dry_run,
        backup=not args.no_backup,
        verbose=args.verbose
    )


def main() -> int:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Manage license headers in source code files",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --check                   # Check all source files
  %(prog)s --add --dry-run           # Preview adding licenses
  %(prog)s --update src/             # Update licenses in src/ directory
  %(prog)s --remove file.py          # Remove license from specific file
        """
    )
    
    # Operation mode (mutually exclusive)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument(
        "--add",
        action="store_true",
        help="Add license headers to files that don't have them"
    )
    group.add_argument(
        "--update", 
        action="store_true",
        help="Update existing license headers to match the license file"
    )
    group.add_argument(
        "--remove",
        action="store_true", 
        help="Remove license headers from files"
    )
    group.add_argument(
        "--check",
        action="store_true",
        help="Check license status of files (default)"
    )
    
    # Config options
    parser.add_argument(
        "--license",
        type=str,
        default="LICENSE",
        help="Path to the license file (default: LICENSE)"
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would be done without making changes"
    )
    parser.add_argument(
        "--no-backup",
        action="store_true",
        help="Don't create backup files when modifying"
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Enable verbose output"
    )
    
    # Target files/directories
    parser.add_argument(
        "paths",
        nargs="*",
        help="Files or directories to process (default: current directory)"
    )
    
    args = parser.parse_args()
    
    # Set logging level
    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)
    
    try:
        # Create config
        config = create_config_from_args(args)
        manager = LicenseManager(config)
        
        # Determine target paths
        target_paths = [Path(p) for p in args.paths] if args.paths else [Path.cwd()]
        
        # Find source files
        source_files = manager.find_source_files(target_paths)
        
        if not source_files:
            logger.warning("No source files found")
            return 0
        
        logger.info(f"Processing {len(source_files)} files...")
        
        # Process files
        results = []
        for filepath in source_files:
            if args.add:
                result = manager.add_license(filepath)
            elif args.update:
                result = manager.update_license(filepath)
            elif args.remove:
                result = manager.remove_license(filepath)
            else:  # check
                result = manager.check_license(filepath)
            
            results.append(result)
            
            if args.verbose or result.status == LicenseStatus.ERROR:
                print(f"{result.filepath}: {result.message}")
        
        # Summary
        status_counts = {}
        modified_count = 0
        error_count = 0
        
        for result in results:
            status_counts[result.status] = status_counts.get(result.status, 0) + 1
            if result.modified:
                modified_count += 1
            if result.status == LicenseStatus.ERROR:
                error_count += 1
        
        print(f"\nSummary:")
        print(f"  Total files processed: {len(results)}")
        for status, count in status_counts.items():
            print(f"  {status.value}: {count}")
        
        if modified_count > 0:
            print(f"  Files modified: {modified_count}")
        
        # Return appropriate exit code
        if args.dry_run:
            # For dry-run mode, always return 0 since no actual changes were made
            # Only return 1 if there were actual processing errors (not license problems)
            return 1 if error_count > 0 else 0
        elif args.check:
            # For check mode, return 1 if any files are missing or have outdated licenses
            problematic_statuses = {LicenseStatus.NO_LICENSE, LicenseStatus.HAS_OUTDATED_LICENSE}
            has_problems = any(result.status in problematic_statuses for result in results)
            return 1 if has_problems else 0
        else:
            return 1 if error_count > 0 else 0
            
    except KeyboardInterrupt:
        logger.info("Operation cancelled by user")
        return 130
    except Exception as e:
        logger.error(f"Fatal error: {e}")
        if args.verbose:
            import traceback
            traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())