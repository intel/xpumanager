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
import fnmatch
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile
from contextlib import contextmanager, suppress
from dataclasses import dataclass
from datetime import datetime
from enum import Enum
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple

# Configure logging
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)
logger = logging.getLogger(__name__)


@contextmanager
def atomic_write(filepath: Path, encoding="utf-8"):
    """Context manager for atomic file writes with automatic cleanup."""
    temp_dir = filepath.parent
    temp_fd, temp_path = tempfile.mkstemp(dir=temp_dir, text=True)
    temp_path = Path(temp_path)

    try:
        os.close(temp_fd)
        with open(temp_path, "w", encoding=encoding) as f:
            yield f

        # Preserve original file permissions
        if filepath.exists():
            stat_info = filepath.stat()
            temp_path.chmod(stat_info.st_mode)

        shutil.move(str(temp_path), str(filepath))

    except Exception:
        with suppress(OSError):
            temp_path.unlink()
        raise


class LicenseStatus(Enum):
    """Enum representing the license status of a file."""

    HAS_VALID_LICENSE = "has_valid_license"
    HAS_OUTDATED_LICENSE = "has_outdated_license"
    HAS_VALID_LICENSE_OLD_YEAR = "has_valid_license_old_year"
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
    update_year: bool = True
    current_year: int = datetime.now().year


class LicenseManager:
    """Manages license headers in source code files."""

    # Default file patterns to process
    DEFAULT_FILE_PATTERNS = [
        "*.py",
        "*.cpp",
        "*.h",
        "*.hpp",
        "*.c",
        "*.cc",
        "*.go",
        "*.sh",
        "*.js",
        "*.ts",
        "*.java",
        "*.rs",
        "*.rb",
        "Makefile",
        "*Dockerfile*",
        "*meson.build*",
    ]

    # Default directories/files to exclude
    DEFAULT_EXCLUDE_PATTERNS = [
        "*.git*",
        "build*",
        "*dist*",
        "*node_modules*",
        "*__pycache__*",
        "*.egg-info*",
        "*venv*",
        "*env*",
        "*third_party*",
        "*docs*",
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
        ".cmake": "#",
        "meson.build": "#",
    }

    # Block comment patterns for detecting old-style licenses
    BLOCK_COMMENT_PATTERNS = {
        ".cpp": (r"/\*", r"\*/"),
        ".h": (r"/\*", r"\*/"),
        ".hpp": (r"/\*", r"\*/"),
        ".c": (r"/\*", r"\*/"),
        ".cc": (r"/\*", r"\*/"),
        ".cxx": (r"/\*", r"\*/"),
    }

    def __init__(self, config: LicenseConfig):
        """Initialize the LicenseManager with config."""
        self.config = config
        self._validate_config()
        self._license_content = self._load_license()

    def _validate_config(self) -> None:
        """Validate the config."""
        if not self.config.license_file.exists():
            raise FileNotFoundError(
                f"License file not found: {self.config.license_file}"
            )

        if not self.config.license_file.is_file():
            raise ValueError(f"License path is not a file: {self.config.license_file}")

    def _load_license(self) -> List[str]:
        """Load and validate the license file content."""
        try:
            with open(self.config.license_file, "r", encoding="utf-8") as f:
                content = f.read().strip().splitlines()

            if not content:
                raise ValueError("License file is empty")

            logger.info(
                f"Loaded license from {self.config.license_file} ({len(content)} lines)"
            )
            return content

        except UnicodeDecodeError as e:
            raise ValueError(f"License file contains invalid UTF-8: {e}")

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
        return len(lines) > 0 and lines[0].startswith("#!")

    def _find_block_comment_license(
        self, content: str, filepath: Path
    ) -> Optional[Tuple[int, int]]:
        """Find license in block comment style (/* */). Returns (start_line, end_line) or None."""
        suffix = filepath.suffix.lower()
        if suffix not in self.BLOCK_COMMENT_PATTERNS:
            return None

        start_pattern, end_pattern = self.BLOCK_COMMENT_PATTERNS[suffix]
        lines = content.splitlines()

        # Look for copyright in block comments
        in_block = False
        block_start = None

        for i, line in enumerate(lines):
            if re.search(start_pattern, line):
                in_block = True
                block_start = i

            if in_block and re.search(r"Copyright", line, re.IGNORECASE):
                # Found copyright in block comment, now find the end
                for j in range(i, len(lines)):
                    if re.search(end_pattern, lines[j]):
                        return (block_start, j)
                return None

            if re.search(end_pattern, line):
                in_block = False
                block_start = None

        return None

    def _get_file_last_modified_year(self, filepath: Path) -> Optional[int]:
        """Get the year when the file was last modified using git if available."""
        # Try git first to get the last commit year for this file
        with suppress(
            subprocess.TimeoutExpired,
            subprocess.SubprocessError,
            ValueError,
            FileNotFoundError,
        ):
            result = subprocess.run(
                [
                    "git",
                    "log",
                    "-1",
                    "--format=%cd",
                    "--date=format:%Y",
                    "--",
                    str(filepath),
                ],
                capture_output=True,
                text=True,
                cwd=filepath.parent,
                timeout=10,
                check=False,
            )
            if result.returncode == 0 and result.stdout.strip():
                return int(result.stdout.strip())

        # Fallback to filesystem modification time
        with suppress(OSError):
            mtime = filepath.stat().st_mtime
            return datetime.fromtimestamp(mtime).year

        return None

    def _extract_copyright_year_from_license(
        self, license_lines: List[str]
    ) -> Optional[str]:
        """Extract the copyright year pattern from license lines."""
        for line in license_lines:
            # Look for patterns like "Copyright (C) 2025" or "Copyright 2025"
            match = re.search(r"Copyright\s*\([Cc]\)\s*(\d{4})", line)
            if not match:
                match = re.search(r"Copyright\s+(\d{4})", line)
            if match:
                return match.group(1)
        return None

    def _create_license_with_year(
        self, filepath: Path, year: int, original_year: Optional[int] = None
    ) -> List[str]:
        """Create license with specified year, optionally creating a year range."""
        formatted_license = self._insert_comments_in_license(filepath)

        # Replace the year in the license
        updated_license = []
        for line in formatted_license:
            if original_year and original_year != year:
                # Create a year range
                year_range = f"{original_year}-{year}"
                line = re.sub(
                    r"(Copyright\s*\([Cc]\)\s*)\d{4}", rf"\g<1>{year_range}", line
                )
                line = re.sub(r"(Copyright\s+)\d{4}", rf"\g<1>{year_range}", line)
            else:
                # Just use the single year
                line = re.sub(r"(Copyright\s*\([Cc]\)\s*)\d{4}", rf"\g<1>{year}", line)
                line = re.sub(r"(Copyright\s+)\d{4}", rf"\g<1>{year}", line)
            updated_license.append(line)

        return updated_license

    def _find_license_with_flexible_year(
        self, content: str, filepath: Path
    ) -> Optional[Tuple[int, int, int]]:
        """Find license in file, allowing for different years. Returns (start_line, end_line, found_year)."""
        content_lines = content.splitlines()

        # Get the template license without year consideration
        template_license = self._insert_comments_in_license(filepath)

        # Search for the license with flexible year matching
        for i in range(len(content_lines)):
            potential_end = i + len(template_license)
            if potential_end <= len(content_lines):
                found_year = None
                matches = True

                for j, (content_line, template_line) in enumerate(
                    zip(content_lines[i:potential_end], template_license)
                ):
                    # Check if this template line contains a copyright year pattern
                    if "Copyright" in template_line and re.search(
                        r"\d{4}", template_line
                    ):
                        # Extract year from content line using a simple regex
                        year_match = re.search(
                            r"Copyright\s*\([Cc]\)\s*(\d{4}(?:-\d{4})?)|Copyright\s+(\d{4}(?:-\d{4})?)",
                            content_line,
                        )
                        if year_match:
                            year_text = year_match.group(1) or year_match.group(2)

                            # Extract the first year from the range or single year
                            if "-" in year_text:
                                found_year = int(year_text.split("-")[0])
                            else:
                                found_year = int(year_text)

                            # Check if the rest of the line matches (without the year)
                            content_without_year = re.sub(
                                r"(\d{4}(?:-\d{4})?)", "XXXX", content_line
                            )
                            template_without_year = re.sub(
                                r"\d{4}", "XXXX", template_line
                            )

                            if content_without_year != template_without_year:
                                matches = False
                                break
                        else:
                            matches = False
                            break
                    else:
                        # Regular line matching (no year pattern)
                        if content_line != template_line:
                            matches = False
                            break

                if matches and found_year:
                    return (i, potential_end - 1, found_year)

        return None

    def _find_license_in_file(
        self, content: str, formatted_license: List[str]
    ) -> Optional[Tuple[int, int]]:
        """Find the lines of the license in a file if one exists. Returns (start_line, end_line) or None."""
        content_lines = content.splitlines()
        license_text = "\n".join(formatted_license)

        if license_text in content:
            for i in range(len(content_lines)):
                potential_end = i + len(formatted_license)
                if potential_end <= len(content_lines):
                    section = "\n".join(content_lines[i:potential_end])
                    if section == license_text:
                        return (i, potential_end - 1)

        return None

    def _determine_license_status(
        self, filepath: Path
    ) -> Tuple[LicenseStatus, Optional[int]]:
        """Detect the current license status of a file. Returns (status, found_year)."""
        try:
            with open(filepath, "r", encoding="utf-8") as f:
                content = f.read()

            # Check for any copyright notice
            copyright_patterns = [
                r"Copyright\s*\([Cc]\)",
                r"Copyright\s*©",
                r"Copyright\s+\d{4}",
                r"Copyright\s+[A-Za-z]+",
            ]

            has_any_copyright = any(
                re.search(pattern, content, re.IGNORECASE)
                for pattern in copyright_patterns
            )

            if not has_any_copyright:
                return LicenseStatus.NO_LICENSE, None

            # Check for block comment style license (old style)
            block_license = self._find_block_comment_license(content, filepath)
            if block_license is not None:
                return LicenseStatus.HAS_OUTDATED_LICENSE, None

            # Check if it matches our license template (with flexible year)
            license_match = self._find_license_with_flexible_year(content, filepath)

            if license_match is not None:
                start_line, end_line, found_year = license_match

                # Check if the year matches current license year
                template_year = self._extract_copyright_year_from_license(
                    self._license_content
                )
                if template_year and found_year == int(template_year):
                    return LicenseStatus.HAS_VALID_LICENSE, found_year
                else:
                    return LicenseStatus.HAS_VALID_LICENSE_OLD_YEAR, found_year
            else:
                return LicenseStatus.HAS_OUTDATED_LICENSE, None

        except UnicodeDecodeError:
            logger.warning(f"Cannot read {filepath} as UTF-8, skipping")
            return LicenseStatus.ERROR, None
        except Exception as e:
            logger.error(f"Error reading {filepath}: {e}")
            return LicenseStatus.ERROR, None

    def _backup_file(self, filepath: Path) -> Optional[Path]:
        """Create a backup of the file. Returns backup path or None if backup failed."""
        if not self.config.backup:
            return None

        backup_path = filepath.with_suffix(filepath.suffix + ".backup")
        with suppress(Exception):
            shutil.copy2(filepath, backup_path)
            logger.debug(f"Created backup: {backup_path}")
            return backup_path

        logger.error(f"Failed to create backup for {filepath}")
        return None

    def _write_file_atomically(self, filepath: Path, content: str) -> bool:
        """Write file content atomically using a context manager."""
        try:
            with atomic_write(filepath) as f:
                f.write(content)
            return True
        except Exception as e:
            logger.error(f"Failed to write {filepath}: {e}")
            return False
    
    def _cleanup_backup(self, backup_path: Optional[Path]) -> None:
        """Remove backup file if it exists."""
        if backup_path and backup_path.exists():
            with suppress(OSError):
                backup_path.unlink()
                logger.debug(f"Removed backup: {backup_path}")

    def add_license(self, filepath: Path) -> FileProcessingResult:
        """Add license header to a file."""
        status, found_year = self._determine_license_status(filepath)

        if status == LicenseStatus.HAS_VALID_LICENSE:
            return FileProcessingResult(
                filepath=filepath, status=status, message="Already has valid license"
            )

        if status == LicenseStatus.ERROR:
            return FileProcessingResult(
                filepath=filepath,
                status=status,
                message="Cannot process file due to encoding or read error",
            )

        try:
            with open(filepath, "r", encoding="utf-8") as f:
                original_content = f.read()

            lines = original_content.splitlines()

            # Determine what year to use
            if self.config.update_year:
                file_year = self._get_file_last_modified_year(filepath)
                license_year = file_year if file_year else self.config.current_year
            else:
                license_year = self.config.current_year

            formatted_license = self._create_license_with_year(filepath, license_year)

            # Determine insertion point
            insert_position = 1 if self._has_shebang(lines) else 0

            # Insert license
            new_lines = (
                lines[:insert_position]
                + formatted_license
                + [""]  # Empty line after license
                + lines[insert_position:]
            )

            # Preserve trailing newline if original had one
            new_content = "\n".join(new_lines)
            if original_content.endswith("\n"):
                new_content += "\n"

            if self.config.dry_run:
                logger.info(
                    f"[DRY RUN] Would add license to {filepath} (year: {license_year})"
                )
                return FileProcessingResult(
                    filepath=filepath,
                    status=LicenseStatus.NO_LICENSE,
                    message=f"Would add license with year {license_year} (dry run)",
                    modified=True,
                )

            # Create backup
            backup_path = self._backup_file(filepath)

            # Write the file
            if self._write_file_atomically(filepath, new_content):
                self._cleanup_backup(backup_path)
                return FileProcessingResult(
                    filepath=filepath,
                    status=LicenseStatus.HAS_VALID_LICENSE,
                    message=f"License added successfully (year: {license_year})",
                    modified=True,
                )
            else:
                return FileProcessingResult(
                    filepath=filepath,
                    status=LicenseStatus.ERROR,
                    message="Failed to write file",
                )

        except Exception as e:
            logger.error(f"Error processing {filepath}: {e}")
            return FileProcessingResult(
                filepath=filepath,
                status=LicenseStatus.ERROR,
                message=f"Processing error: {e}",
            )

    def remove_license(self, filepath: Path) -> FileProcessingResult:
        """Remove license header from a file."""
        status, found_year = self._determine_license_status(filepath)

        if status == LicenseStatus.NO_LICENSE:
            return FileProcessingResult(
                filepath=filepath, status=status, message="No license to remove"
            )

        if status == LicenseStatus.ERROR:
            return FileProcessingResult(
                filepath=filepath,
                status=status,
                message="Cannot process file due to encoding or read error",
            )

        try:
            with open(filepath, "r", encoding="utf-8") as f:
                content = f.read()

            # First check for block comment style license
            block_license = self._find_block_comment_license(content, filepath)
            if block_license is not None:
                start_line, end_line = block_license
            else:
                # Use the flexible year finder for single-line comment licenses
                license_match = self._find_license_with_flexible_year(content, filepath)

                if license_match is None:
                    # Fallback to old method for outdated licenses
                    formatted_license = self._insert_comments_in_license(filepath)
                    license_location = self._find_license_in_file(
                        content, formatted_license
                    )
                    if license_location is None:
                        return FileProcessingResult(
                            filepath=filepath,
                            status=LicenseStatus.HAS_OUTDATED_LICENSE,
                            message="Has copyright but not our exact license format",
                        )
                    start_line, end_line = license_location
                else:
                    start_line, end_line, _ = license_match

            lines = content.splitlines()

            # Remove license and any following empty lines
            new_lines = lines[:start_line] + lines[end_line + 1 :]

            # Remove leading empty lines that might be left after license removal
            while new_lines and not new_lines[0].strip():
                new_lines.pop(0)

            # Preserve trailing newline if original had one
            new_content = "\n".join(new_lines)
            if content.endswith("\n"):
                new_content += "\n"

            if self.config.dry_run:
                logger.info(f"[DRY RUN] Would remove license from {filepath}")
                return FileProcessingResult(
                    filepath=filepath,
                    status=status,
                    message="Would remove license (dry run)",
                    modified=True,
                )

            # Create backup
            backup_path = self._backup_file(filepath)

            # Write the file
            if self._write_file_atomically(filepath, new_content):
                self._cleanup_backup(backup_path)
                return FileProcessingResult(
                    filepath=filepath,
                    status=LicenseStatus.NO_LICENSE,
                    message="License removed successfully",
                    modified=True,
                )
            else:
                return FileProcessingResult(
                    filepath=filepath,
                    status=LicenseStatus.ERROR,
                    message="Failed to write file",
                )

        except Exception as e:
            logger.error(f"Error processing {filepath}: {e}")
            return FileProcessingResult(
                filepath=filepath,
                status=LicenseStatus.ERROR,
                message=f"Processing error: {e}",
            )

    def update_license(self, filepath: Path) -> FileProcessingResult:
        """Update license header in a file."""
        status, found_year = self._determine_license_status(filepath)

        if status == LicenseStatus.HAS_VALID_LICENSE:
            return FileProcessingResult(
                filepath=filepath, status=status, message="Already has current license"
            )

        if status == LicenseStatus.ERROR:
            return FileProcessingResult(
                filepath=filepath,
                status=status,
                message="Cannot process file due to encoding or read error",
            )

        if status == LicenseStatus.NO_LICENSE:
            return self.add_license(filepath)

        if status == LicenseStatus.HAS_VALID_LICENSE_OLD_YEAR:
            # Update year if file has been modified since the found year
            if self.config.update_year:
                file_year = self._get_file_last_modified_year(filepath)
                if file_year and file_year > found_year:
                    return self._update_license_year(filepath, file_year)
                else:
                    return FileProcessingResult(
                        filepath=filepath,
                        status=LicenseStatus.HAS_VALID_LICENSE_OLD_YEAR,
                        message=f"License year {found_year} is still current for this file",
                    )
            else:
                # Force update to current year
                return self._update_license_year(filepath, self.config.current_year)

        # For HAS_OUTDATED_LICENSE, remove and re-add
        remove_result = self.remove_license(filepath)
        if remove_result.status == LicenseStatus.ERROR:
            return remove_result

        return self.add_license(filepath)

    def _update_license_year(
        self, filepath: Path, new_year: int
    ) -> FileProcessingResult:
        """Update just the year in an existing license."""
        try:
            with open(filepath, "r", encoding="utf-8") as f:
                content = f.read()

            license_match = self._find_license_with_flexible_year(content, filepath)
            if not license_match:
                return FileProcessingResult(
                    filepath=filepath,
                    status=LicenseStatus.ERROR,
                    message="Could not locate license to update",
                )

            start_line, end_line, old_year = license_match
            lines = content.splitlines()

            # Create new license with updated year (creating a year range)
            new_license = self._create_license_with_year(filepath, new_year, old_year)

            # Replace the license section
            new_lines = lines[:start_line] + new_license + lines[end_line + 1 :]
            
            # Preserve trailing newline if original had one
            new_content = "\n".join(new_lines)
            if content.endswith("\n"):
                new_content += "\n"

            if self.config.dry_run:
                year_display = (
                    f"{old_year}-{new_year}" if old_year != new_year else str(new_year)
                )
                logger.info(
                    f"[DRY RUN] Would update license year in {filepath} from {old_year} to {year_display}"
                )
                return FileProcessingResult(
                    filepath=filepath,
                    status=LicenseStatus.HAS_VALID_LICENSE_OLD_YEAR,
                    message=f"Would update year from {old_year} to {year_display} (dry run)",
                    modified=True,
                )

            # Create backup
            backup_path = self._backup_file(filepath)

            # Write the file
            if self._write_file_atomically(filepath, new_content):
                self._cleanup_backup(backup_path)
                year_display = (
                    f"{old_year}-{new_year}" if old_year != new_year else str(new_year)
                )
                return FileProcessingResult(
                    filepath=filepath,
                    status=LicenseStatus.HAS_VALID_LICENSE,
                    message=f"License year updated from {old_year} to {year_display}",
                    modified=True,
                )
            else:
                return FileProcessingResult(
                    filepath=filepath,
                    status=LicenseStatus.ERROR,
                    message="Failed to write file",
                )

        except Exception as e:
            logger.error(f"Error updating license year in {filepath}: {e}")
            return FileProcessingResult(
                filepath=filepath,
                status=LicenseStatus.ERROR,
                message=f"Processing error: {e}",
            )

    def check_license(self, filepath: Path) -> FileProcessingResult:
        """Check license status of a file."""
        status, found_year = self._determine_license_status(filepath)

        messages = {
            LicenseStatus.HAS_VALID_LICENSE: "Has valid license",
            LicenseStatus.HAS_OUTDATED_LICENSE: "Has outdated or different license",
            LicenseStatus.HAS_VALID_LICENSE_OLD_YEAR: f"Has valid license but old year ({found_year or 'unknown'})",
            LicenseStatus.NO_LICENSE: "Missing license header",
            LicenseStatus.ERROR: "Cannot process file",
        }

        message = messages[status]
        if (
            status == LicenseStatus.HAS_VALID_LICENSE_OLD_YEAR
            and self.config.update_year
        ):
            file_year = self._get_file_last_modified_year(filepath)
            if file_year and found_year and file_year > found_year:
                message += f" - file modified in {file_year}, should update"
            else:
                message += " - but file hasn't been modified since then"

        return FileProcessingResult(filepath=filepath, status=status, message=message)

    def _get_git_modified_files(self, base_ref: str = "origin/main") -> Set[Path]:
        """Get list of files modified compared to the base reference."""
        repo_root = Path.cwd()
        modified_files = set()

        # Helper to process git output
        def add_files_from_output(output: str) -> None:
            for line in output.strip().splitlines():
                if line.strip():
                    file_path = repo_root / line.strip()
                    if file_path.exists() and file_path.is_file():
                        modified_files.add(file_path)

        # Get different types of changes
        git_commands = [
            ["git", "diff", "--name-only", base_ref, "HEAD"],
            ["git", "diff", "--name-only"],
            ["git", "diff", "--cached", "--name-only"],
        ]

        for cmd in git_commands:
            with suppress(subprocess.TimeoutExpired, subprocess.SubprocessError):
                result = subprocess.run(
                    cmd,
                    capture_output=True,
                    text=True,
                    cwd=repo_root,
                    timeout=30,
                    check=False,
                )
                if result.returncode == 0:
                    add_files_from_output(result.stdout)

        if not modified_files:
            logger.info("No modified files found in git")

        return modified_files

    def find_source_files(
        self,
        paths: List[Path],
        git_modified_only: bool = False,
        git_base_ref: str = "origin/main",
    ) -> List[Path]:
        """Find all source files matching the configured patterns."""
        if git_modified_only:
            # Get modified files from git
            modified_files = self._get_git_modified_files(git_base_ref)
            if not modified_files:
                logger.info("No modified files found in git")
                return []

            # Filter modified files to only include source files
            source_files = set()
            for filepath in modified_files:
                # Check if file matches any of our file patterns
                for pattern in self.config.file_patterns:
                    if filepath.match(pattern):
                        source_files.add(filepath)
                        break
        else:
            # Original behavior - scan directories
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
            should_exclude = False
            # Convert to string and normalize path separators
            filepath_str = str(filepath).replace(os.sep, "/")

            for pattern in self.config.exclude_patterns:
                # Normalize pattern
                normalized_pattern = pattern.replace(os.sep, "/")

                # Check if the full path matches the pattern
                if fnmatch.fnmatch(filepath_str, normalized_pattern):
                    should_exclude = True
                    break

                # Also check if any part of the path contains the pattern (for patterns like "*third_party*")
                if "*" in pattern:
                    # Remove leading and trailing asterisks for substring matching
                    substring = pattern.strip("*")
                    if substring and substring in filepath_str:
                        should_exclude = True
                        break

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
        verbose=args.verbose,
        update_year=args.update_year,
        current_year=args.current_year,
    )


def main() -> int:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Manage license headers in source code files",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --check                                    # Check all source files
  %(prog)s --check --git-modified-only                # Check only files modified vs origin/main
  %(prog)s --check --git-modified-only --git-base-ref origin/dev  # Check vs different branch
  %(prog)s --add --dry-run                            # Preview adding licenses
  %(prog)s --update src/                              # Update licenses in src/ directory
  %(prog)s --remove file.py                           # Remove license from specific file
  %(prog)s --update --no-update-year                  # Update licenses but keep original years
        """,
    )

    # Operation mode (mutually exclusive)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument(
        "--add",
        action="store_true",
        help="Add license headers to files that don't have them",
    )
    group.add_argument(
        "--update",
        action="store_true",
        help="Update existing license headers to match the license file",
    )
    group.add_argument(
        "--remove", action="store_true", help="Remove license headers from files"
    )
    group.add_argument(
        "--check", action="store_true", help="Check license status of files (default)"
    )

    # Config options
    parser.add_argument(
        "--license",
        type=str,
        default="LICENSE",
        help="Path to the license file (default: LICENSE)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would be done without making changes",
    )
    parser.add_argument(
        "--no-backup",
        action="store_true",
        help="Don't create backup files when modifying",
    )
    parser.add_argument(
        "--verbose", "-v", action="store_true", help="Enable verbose output"
    )
    parser.add_argument(
        "--no-update-year",
        action="store_false",
        dest="update_year",
        help="Don't update copyright year based on file modification time",
    )
    parser.add_argument(
        "--current-year",
        type=int,
        default=datetime.now().year,
        help="Year to use for new licenses (default: current year)",
    )
    parser.add_argument(
        "--git-modified-only",
        action="store_true",
        help="Only process files modified compared to git base reference (default: process all files)",
    )
    parser.add_argument(
        "--git-base-ref",
        type=str,
        default="origin/main",
        help="Git reference to compare against when using --git-modified-only (default: origin/main)",
    )

    # Target files/directories
    parser.add_argument(
        "paths",
        nargs="*",
        help="Files or directories to process (default: current directory)",
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
        if args.git_modified_only:
            logger.info(f"Checking only files modified compared to {args.git_base_ref}")

        source_files = manager.find_source_files(
            target_paths,
            git_modified_only=args.git_modified_only,
            git_base_ref=args.git_base_ref,
        )

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

        print("\nSummary:")
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
            problematic_statuses = {
                LicenseStatus.NO_LICENSE,
                LicenseStatus.HAS_OUTDATED_LICENSE,
            }
            has_problems = any(
                result.status in problematic_statuses for result in results
            )
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
