#!/usr/bin/env python3
# Copyright (C) 2025-2026 Intel Corporation
# SPDX-License-Identifier: MIT

"""
SPDX License header management tool for source code files.

This script provides functionality to add, update, remove, and check SPDX-style
license headers in source code files across multiple programming languages.

Supports:
- Adding licenses to files without them
- Updating licenses with incorrect format
- Removing licenses from files
- Checking license status
- Automatic copyright year updates (e.g., 2025 -> 2025-2026)
- SPDX-License-Identifier format
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
    GENERATED = "generated"
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

    company: str = "Intel Corporation"
    spdx_id: str = "MIT"
    comment_styles: Dict[str, Tuple[str, str, str]] = None
    file_patterns: List[str] = None
    exclude_patterns: List[str] = None
    dry_run: bool = False
    backup: bool = True
    verbose: bool = False
    update_year: bool = True
    current_year: int = datetime.now().year

    def __post_init__(self):
        if self.comment_styles is None:
            self.comment_styles = SPDXLicenseManager.DEFAULT_COMMENT_STYLES.copy()
        if self.file_patterns is None:
            self.file_patterns = SPDXLicenseManager.DEFAULT_FILE_PATTERNS.copy()
        if self.exclude_patterns is None:
            self.exclude_patterns = SPDXLicenseManager.DEFAULT_EXCLUDE_PATTERNS.copy()


class SPDXLicenseManager:
    """Manages SPDX-style license headers in source code files."""

    # Default file patterns to process
    DEFAULT_FILE_PATTERNS = [
        "*.py",
        "*.cpp",
        "*.h",
        "*.hpp",
        "*.c",
        "*.cc",
        "*.cxx",
        "*.go",
        "*.sh",
        "*.js",
        "*.ts",
        "*.java",
        "*.rs",
        "*.rb",
        "*.pl",
        "*.r",
        "Makefile",
        "*.mk",
        "*Dockerfile*",
        "*meson.build*",
        "*.cmake",
        "CMakeLists.txt",
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
        "*vendor*",
        "*.backup",
        "*conanrun.sh",
        "*deactivate_conanrun.sh",
        "*conanbuild*",
    ]

    # Comment styles: (start, middle, end)
    # For single-line comments, middle is the comment prefix
    # For block comments, start is opening, middle is continuation, end is closing
    DEFAULT_COMMENT_STYLES = {
        # Single-line comment languages (middle is the prefix)
        ".py": ("", "#", ""),
        ".sh": ("", "#", ""),
        ".conf": ("", "#", ""),
        ".yml": ("", "#", ""),
        ".yaml": ("", "#", ""),
        ".toml": ("", "#", ""),
        ".ini": ("", "#", ""),
        ".rb": ("", "#", ""),
        ".pl": ("", "#", ""),
        ".r": ("", "#", ""),
        "Dockerfile": ("", "#", ""),
        "Makefile": ("", "#", ""),
        ".mk": ("", "#", ""),
        ".cmake": ("", "#", ""),
        ".generate": ("", "#", ""),
        "CMakeLists.txt": ("", "#", ""),
        "meson.build": ("", "#", ""),
        # Block comment languages (C-style)
        ".cpp": ("/*", " *", " */"),
        ".h": ("/*", " *", " */"),
        ".hpp": ("/*", " *", " */"),
        ".c": ("/*", " *", " */"),
        ".cc": ("/*", " *", " */"),
        ".cxx": ("/*", " *", " */"),
        ".go": ("", "//", ""),
        ".js": ("/*", " *", " */"),
        ".ts": ("/*", " *", " */"),
        ".java": ("/*", " *", " */"),
        ".rs": ("/*", " *", " */"),
    }

    def __init__(self, config: LicenseConfig):
        """Initialize the SPDXLicenseManager with config."""
        self.config = config

    def _get_comment_style(self, filepath: Path) -> Tuple[str, str, str]:
        """Determine the comment style for a file. Returns (start, middle, end)."""
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

    def _create_spdx_license(
        self, filepath: Path, year: int, original_year: Optional[int] = None
    ) -> List[str]:
        """Create SPDX-style license header for a file."""
        start, middle, end = self._get_comment_style(filepath)

        # Determine the year string
        if original_year and original_year != year:
            year_str = f"{original_year}-{year}"
        else:
            year_str = str(year)

        # Build the license lines
        lines = []

        if start:  # Block comment style (e.g., /* */)
            lines.append(start)
            lines.append(f"{middle} Copyright (C) {year_str} {self.config.company}")
            lines.append(f"{middle} SPDX-License-Identifier: {self.config.spdx_id}")
            lines.append(f"{middle}")
            lines.append(end)
        else:  # Single-line comment style (e.g., #)
            lines.append(f"{middle} Copyright (C) {year_str} {self.config.company}")
            lines.append(f"{middle} SPDX-License-Identifier: {self.config.spdx_id}")

        return lines

    def _has_shebang(self, lines: List[str]) -> bool:
        """Check if the file starts with a shebang."""
        return len(lines) > 0 and lines[0].startswith("#!")

    def _get_file_last_modified_year(self, filepath: Path) -> Optional[int]:
        """Get the year when the file was last modified using git if available."""
        # Try git first to get the last commit year for this file
        with suppress(
            subprocess.TimeoutExpired,
            subprocess.SubprocessError,
            ValueError,
            FileNotFoundError,
        ):
            # Convert to absolute path and run git from repository root
            abs_filepath = filepath.resolve()
            
            # Find git repository root
            result = subprocess.run(
                ["git", "rev-parse", "--show-toplevel"],
                capture_output=True,
                text=True,
                cwd=filepath.parent if filepath.parent.exists() else Path.cwd(),
                timeout=5,
                check=False,
            )
            
            git_root = Path(result.stdout.strip()) if result.returncode == 0 else Path.cwd()
            
            # Get the relative path from git root
            try:
                rel_path = abs_filepath.relative_to(git_root)
            except ValueError:
                # If file is not under git root, use absolute path
                rel_path = abs_filepath
            
            # Get last commit year for this file
            result = subprocess.run(
                [
                    "git",
                    "log",
                    "-1",
                    "--format=%cd",  # Use committer date (when committed to repo)
                    "--date=format:%Y",
                    "--",
                    str(rel_path),
                ],
                capture_output=True,
                text=True,
                cwd=git_root,
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

    def _find_spdx_license(
        self, content: str, filepath: Path
    ) -> Optional[Tuple[int, int, Optional[int]]]:
        """
        Find SPDX license in file content.
        Returns (start_line, end_line, found_year) or None.
        """
        lines = content.splitlines()
        start, middle, end = self._get_comment_style(filepath)

        # Patterns to match - support both (C) and © symbols
        copyright_pattern = re.compile(
            r"Copyright\s*(?:\([Cc]\)|©)\s*(\d{4}(?:-\d{4})?)\s+" + re.escape(self.config.company)
        )
        spdx_pattern = re.compile(
            r"SPDX-License-Identifier:\s*" + re.escape(self.config.spdx_id)
        )

        found_copyright_line = None
        found_spdx_line = None
        found_year = None
        potential_start = None
        potential_end = None

        for i, line in enumerate(lines):
            # Check for copyright
            copyright_match = copyright_pattern.search(line)
            if copyright_match and found_copyright_line is None:
                found_copyright_line = i
                if potential_start is None:
                    # For block comments, check if previous lines contain the opening
                    # Look back up to 3 lines to handle blank lines between /* and Copyright
                    if start:
                        for lookback in range(1, min(4, i + 1)):
                            if i >= lookback and start.strip() in lines[i - lookback]:
                                potential_start = i - lookback
                                break
                        else:
                            potential_start = i
                    else:
                        potential_start = i

                # Extract year (use the end year from range if present)
                year_str = copyright_match.group(1)
                if "-" in year_str:
                    # For ranges like "2021-2023", use the end year (2023)
                    found_year = int(year_str.split("-")[1])
                else:
                    found_year = int(year_str)

            # Check for SPDX identifier
            if spdx_pattern.search(line) and found_spdx_line is None:
                found_spdx_line = i
                if potential_start is None:
                    potential_start = i

            # If we have both, find the end
            if found_copyright_line is not None and found_spdx_line is not None:
                if potential_end is None:
                    # For block comments, look for closing
                    if end:
                        for j in range(max(found_copyright_line, found_spdx_line), len(lines)):
                            if end in lines[j]:
                                potential_end = j
                                break
                        # Also check for empty comment line before closing
                        if potential_end and potential_end > 0:
                            if lines[potential_end - 1].strip() == middle.strip():
                                pass  # Include empty line before closing
                        # Fallback: file may use single-line comment style (e.g. //)
                        # even though the extension is mapped to block style (e.g. /* */)
                        if potential_end is None:
                            potential_end = max(found_copyright_line, found_spdx_line)
                    else:
                        # For single-line comments, end is the SPDX line
                        potential_end = max(found_copyright_line, found_spdx_line)

                if potential_start is not None and potential_end is not None:
                    return (potential_start, potential_end, found_year)

        return None

    def _find_any_copyright(self, content: str) -> bool:
        """Check if file has any copyright notice."""
        copyright_patterns = [
            r"Copyright\s*\([Cc]\)",
            r"Copyright\s*©",
            r"Copyright\s+\d{4}",
        ]
        return any(re.search(pattern, content, re.IGNORECASE) for pattern in copyright_patterns)

    def _extract_year_from_copyright(self, content: str) -> Optional[int]:
        """Extract the copyright year from any copyright notice."""
        # Match various copyright formats and extract year or year range
        copyright_patterns = [
            r"Copyright\s*(?:\([Cc]\)|©)\s*(\d{4}(?:-\d{4})?)\s+" + re.escape(self.config.company),
            r"Copyright\s*(?:\([Cc]\)|©)\s*(\d{4}(?:-\d{4})?)",
            r"Copyright\s+(\d{4}(?:-\d{4})?)",
        ]
        
        for pattern in copyright_patterns:
            match = re.search(pattern, content, re.IGNORECASE)
            if match:
                year_str = match.group(1)
                if "-" in year_str:
                    # For ranges like "2021-2023", use the end year (2023)
                    return int(year_str.split("-")[1])
                else:
                    return int(year_str)
        
        return None

    def _find_old_style_license(self, content: str, filepath: Path) -> Optional[Tuple[int, int]]:
        """
        Find old-style MIT license block (non-SPDX).
        Returns (start_line, end_line) or None.
        
        This is simplified since we just need to detect and remove old licenses,
        not parse them perfectly. SPDX format is much cleaner.
        """
        lines = content.splitlines()

        # Look for typical full MIT license text (non-SPDX)
        mit_indicators = [
            r"Permission is hereby granted",
            r"THE SOFTWARE IS PROVIDED",
        ]

        # Find start (copyright line)
        start_line = None
        indicator_line = None
        
        for i, line in enumerate(lines):
            if re.search(r"Copyright", line, re.IGNORECASE) and start_line is None:
                start_line = i
                # Check if there's a block comment start before it
                if i > 0 and '/*' in lines[i - 1]:
                    start_line = i - 1
            
            # Check for MIT license indicators
            if start_line is not None:
                for indicator in mit_indicators:
                    if re.search(indicator, line, re.IGNORECASE):
                        indicator_line = i
                        break
                if indicator_line:
                    break

        if start_line is None or indicator_line is None:
            return None

        # Find end (look for closing comment or blank line after license text)
        end_line = indicator_line
        for i in range(indicator_line, min(indicator_line + 25, len(lines))):
            if '*/' in lines[i]:
                end_line = i
                break
            # Look for end of license block (blank line or non-comment)
            if i > indicator_line + 5:  # After sufficient license text
                line = lines[i].strip()
                if not line or not any(c in line for c in ['*', '#', '/']):
                    end_line = i - 1
                    break

        return (start_line, end_line)

    def _is_generated_file(self, content: str) -> bool:
        """Check if a file is auto-generated and should be exempt from license requirements."""
        generated_patterns = [
            r"Code generated by",
        ]
        first_lines = "\n".join(content.splitlines()[:5])
        return any(re.search(p, first_lines, re.IGNORECASE) for p in generated_patterns)

    def _determine_license_status(
        self, filepath: Path
    ) -> Tuple[LicenseStatus, Optional[int]]:
        """Detect the current license status of a file. Returns (status, found_year)."""
        try:
            with open(filepath, "r", encoding="utf-8") as f:
                content = f.read()

            # Skip license check for auto-generated files
            if self._is_generated_file(content):
                return LicenseStatus.GENERATED, None

            # Check for any copyright notice
            if not self._find_any_copyright(content):
                return LicenseStatus.NO_LICENSE, None

            # Check for SPDX license
            spdx_match = self._find_spdx_license(content, filepath)
            if spdx_match:
                start_line, end_line, found_year = spdx_match

                # Check if year needs updating
                if self.config.update_year and found_year:
                    # Compare against current year, not git modification year
                    # This ensures we always flag licenses that don't have current year
                    if found_year < self.config.current_year:
                        return LicenseStatus.HAS_VALID_LICENSE_OLD_YEAR, found_year
                    else:
                        return LicenseStatus.HAS_VALID_LICENSE, found_year
                else:
                    return LicenseStatus.HAS_VALID_LICENSE, found_year

            # Check for old-style license
            if self._find_old_style_license(content, filepath):
                # Extract year from the old license
                old_year = self._extract_year_from_copyright(content)
                return LicenseStatus.HAS_OUTDATED_LICENSE, old_year

            # Has copyright but not our format
            old_year = self._extract_year_from_copyright(content)
            return LicenseStatus.HAS_OUTDATED_LICENSE, old_year

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
        try:
            shutil.copy2(filepath, backup_path)
            logger.debug(f"Created backup: {backup_path}")
            return backup_path
        except Exception as e:
            logger.warning(f"Failed to create backup for {filepath}: {e}")
            return None

    def _write_file_atomically(self, filepath: Path, content: str) -> bool:
        """Write file content atomically."""
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

    def add_license(self, filepath: Path, original_year: Optional[int] = None) -> FileProcessingResult:
        """Add SPDX license header to a file.
        
        Args:
            filepath: Path to the file to add license to
            original_year: Optional original copyright year to preserve when updating
        """
        status, found_year = self._determine_license_status(filepath)

        if status == LicenseStatus.HAS_VALID_LICENSE:
            return FileProcessingResult(
                filepath=filepath, status=status, message="Already has valid SPDX license"
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

            # Determine year to use
            if self.config.update_year:
                file_year = self._get_file_last_modified_year(filepath)
                license_year = file_year if file_year else self.config.current_year
            else:
                license_year = self.config.current_year

            # Create license (preserve original year if provided)
            license_lines = self._create_spdx_license(
                filepath, license_year, original_year=original_year
            )

            # Determine insertion point
            insert_position = 1 if self._has_shebang(lines) else 0

            # Insert license
            new_lines = (
                lines[:insert_position]
                + license_lines
                + [""]  # Empty line after license
                + lines[insert_position:]
            )

            # Preserve trailing newline
            new_content = "\n".join(new_lines)
            if original_content.endswith("\n"):
                new_content += "\n"

            if self.config.dry_run:
                logger.info(
                    f"[DRY RUN] Would add SPDX license to {filepath} (year: {license_year})"
                )
                return FileProcessingResult(
                    filepath=filepath,
                    status=LicenseStatus.NO_LICENSE,
                    message=f"Would add SPDX license with year {license_year} (dry run)",
                    modified=True,
                )

            # Create backup and write
            backup_path = self._backup_file(filepath)

            if self._write_file_atomically(filepath, new_content):
                self._cleanup_backup(backup_path)
                return FileProcessingResult(
                    filepath=filepath,
                    status=LicenseStatus.HAS_VALID_LICENSE,
                    message=f"SPDX license added successfully (year: {license_year})",
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

            lines = content.splitlines()

            # Try to find SPDX license first
            spdx_match = self._find_spdx_license(content, filepath)
            if spdx_match:
                start_line, end_line, _ = spdx_match
            else:
                # Try to find old-style license
                old_match = self._find_old_style_license(content, filepath)
                if old_match:
                    start_line, end_line = old_match
                else:
                    return FileProcessingResult(
                        filepath=filepath,
                        status=LicenseStatus.HAS_OUTDATED_LICENSE,
                        message="Has copyright but cannot determine license boundaries",
                    )

            # Remove license lines
            new_lines = lines[:start_line] + lines[end_line + 1 :]

            # Remove leading empty lines
            while new_lines and not new_lines[0].strip():
                new_lines.pop(0)

            # Preserve trailing newline
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

            # Create backup and write
            backup_path = self._backup_file(filepath)

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
                filepath=filepath, status=status, message="Already has current SPDX license"
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
            return self._update_license_year(filepath, found_year)

        # For outdated license, remove and re-add (preserve original year)
        remove_result = self.remove_license(filepath)
        if remove_result.status == LicenseStatus.ERROR:
            return remove_result

        return self.add_license(filepath, original_year=found_year)

    def _update_license_year(
        self, filepath: Path, old_year: Optional[int]
    ) -> FileProcessingResult:
        """Update just the year in an existing SPDX license."""
        try:
            with open(filepath, "r", encoding="utf-8") as f:
                content = f.read()

            spdx_match = self._find_spdx_license(content, filepath)
            if not spdx_match:
                return FileProcessingResult(
                    filepath=filepath,
                    status=LicenseStatus.ERROR,
                    message="Could not locate SPDX license to update",
                )

            start_line, end_line, found_year = spdx_match
            lines = content.splitlines()

            # Always use current year for updates to ensure licenses stay current
            new_year = self.config.current_year

            # If years are the same, nothing to do
            if found_year and found_year == new_year:
                return FileProcessingResult(
                    filepath=filepath,
                    status=LicenseStatus.HAS_VALID_LICENSE,
                    message=f"License year {found_year} is already current",
                )

            # Create new license with updated year
            new_license = self._create_spdx_license(filepath, new_year, found_year)

            # Replace the license section
            new_lines = lines[:start_line] + new_license + lines[end_line + 1 :]

            # Preserve trailing newline
            new_content = "\n".join(new_lines)
            if content.endswith("\n"):
                new_content += "\n"

            if self.config.dry_run:
                year_display = (
                    f"{found_year}-{new_year}" if found_year and found_year != new_year else str(new_year)
                )
                logger.info(
                    f"[DRY RUN] Would update license year in {filepath} to {year_display}"
                )
                return FileProcessingResult(
                    filepath=filepath,
                    status=LicenseStatus.HAS_VALID_LICENSE_OLD_YEAR,
                    message=f"Would update year to {year_display} (dry run)",
                    modified=True,
                )

            # Create backup and write
            backup_path = self._backup_file(filepath)

            if self._write_file_atomically(filepath, new_content):
                self._cleanup_backup(backup_path)
                year_display = (
                    f"{found_year}-{new_year}" if found_year and found_year != new_year else str(new_year)
                )
                return FileProcessingResult(
                    filepath=filepath,
                    status=LicenseStatus.HAS_VALID_LICENSE,
                    message=f"License year updated to {year_display}",
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
            LicenseStatus.HAS_VALID_LICENSE: f"Has valid SPDX license (year: {found_year or 'unknown'})",
            LicenseStatus.HAS_OUTDATED_LICENSE: "Has outdated or non-SPDX license",
            LicenseStatus.HAS_VALID_LICENSE_OLD_YEAR: f"Has valid SPDX license but old year ({found_year or 'unknown'})",
            LicenseStatus.NO_LICENSE: "Missing license header",
            LicenseStatus.GENERATED: "Auto-generated file, license check skipped",
            LicenseStatus.ERROR: "Cannot process file",
        }

        message = messages[status]
        if status == LicenseStatus.HAS_VALID_LICENSE_OLD_YEAR and self.config.update_year:
            message += f" - current year is {self.config.current_year}, should update"

        return FileProcessingResult(filepath=filepath, status=status, message=message)

    def _get_git_modified_files(self, base_ref: str = "origin/main") -> Set[Path]:
        """Get list of files modified compared to the base reference."""
        repo_root = Path.cwd()
        modified_files = set()

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

        return modified_files

    def find_source_files(
        self,
        paths: List[Path],
        git_modified_only: bool = False,
        git_base_ref: str = "origin/new_xpum",
    ) -> List[Path]:
        """Find all source files matching the configured patterns."""
        if git_modified_only:
            # Get modified files from git
            modified_files = self._get_git_modified_files(git_base_ref)
            if not modified_files:
                return []
            
            # Filter to only include source files
            source_files = set()
            for filepath in modified_files:
                for pattern in self.config.file_patterns:
                    if filepath.match(pattern):
                        source_files.add(filepath)
                        break
        else:
            # Scan directories
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
            filepath_str = str(filepath).replace(os.sep, "/")
            filepath_parts = filepath_str.split("/")

            for pattern in self.config.exclude_patterns:
                normalized_pattern = pattern.replace(os.sep, "/")

                # Full path glob matching
                if fnmatch.fnmatch(filepath_str, normalized_pattern):
                    should_exclude = True
                    break

                # Check if pattern matches any path component
                if "*" in pattern:
                    pattern_stripped = pattern.strip("*")
                    if pattern_stripped:
                        for part in filepath_parts:
                            if fnmatch.fnmatch(part, pattern):
                                should_exclude = True
                                break
                    if should_exclude:
                        break

            if not should_exclude:
                filtered_files.append(filepath)

        return sorted(filtered_files)


def main() -> int:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Manage SPDX-style license headers in source code files",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --check                                    # Check all source files
  %(prog)s --check --git-modified-only                # Check only files modified vs origin/main
  %(prog)s --add --dry-run                            # Preview adding licenses
  %(prog)s --update src/                              # Update licenses in src/ directory
  %(prog)s --remove file.py                           # Remove license from specific file
  %(prog)s --update --company "My Company" --spdx-id Apache-2.0  # Use different license
        """,
    )

    # Operation mode (mutually exclusive)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument(
        "--add",
        action="store_true",
        help="Add SPDX license headers to files that don't have them",
    )
    group.add_argument(
        "--update",
        action="store_true",
        help="Update existing license headers to SPDX format",
    )
    group.add_argument(
        "--remove", action="store_true", help="Remove license headers from files"
    )
    group.add_argument(
        "--check", action="store_true", help="Check license status of files"
    )

    # License configuration
    parser.add_argument(
        "--company",
        type=str,
        default="Intel Corporation",
        help="Company name for copyright (default: Intel Corporation)",
    )
    parser.add_argument(
        "--spdx-id",
        type=str,
        default="MIT",
        help="SPDX license identifier (default: MIT)",
    )

    # Processing options
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
        help="Only process files modified compared to git base reference",
    )
    parser.add_argument(
        "--git-base-ref",
        type=str,
        default="origin/main",
        help="Git reference to compare against (default: origin/main)",
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
        config = LicenseConfig(
            company=args.company,
            spdx_id=args.spdx_id,
            dry_run=args.dry_run,
            backup=not args.no_backup,
            verbose=args.verbose,
            update_year=args.update_year,
            current_year=args.current_year,
        )
        manager = SPDXLicenseManager(config)

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
            return 1 if error_count > 0 else 0
        elif args.check:
            # Missing license or wrong license format are blocking issues.
            # Old copyright year is informational only — use --update to fix years.
            problematic_statuses = {
                LicenseStatus.NO_LICENSE,
                LicenseStatus.HAS_OUTDATED_LICENSE,
            }
            has_problems = any(
                result.status in problematic_statuses for result in results
            )
            return 1 if has_problems else 0
        else:
            # Add/update/remove modes
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
