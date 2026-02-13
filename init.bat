@echo off
REM UnrealCV Documentation Harness - Environment Check Script
REM Run this at the start of every documentation session

echo ============================================
echo UnrealCV Documentation Environment Check
echo ============================================

REM 1. Verify working directory
cd /d "%~dp0"
echo [1] Working directory: %CD%

REM 2. Check Python availability
echo [2] Python version:
python --version 2>nul || echo    Python not found (ok for doc-only tasks)

REM 3. Check required files exist
echo [3] Required files:
if exist "unrealcv-doc-features.json" (
    echo    unrealcv-doc-features.json: OK
) else (
    echo    unrealcv-doc-features.json: MISSING!
)

if exist "claude-progress.txt" (
    echo    claude-progress.txt: OK
) else (
    echo    claude-progress.txt: MISSING!
)

if exist "docs/" (
    echo    docs/: OK
) else (
    echo    docs/: MISSING!
)

REM 4. Check docs structure
echo [4] Documentation structure:
if exist "docs\index.md" (
    echo    docs/index.md: OK
)
if exist "docs\reference\" (
    echo    docs/reference/: OK
)
if exist "docs\api\" (
    echo    docs/api/: OK
)
if exist "docs\guides\" (
    echo    docs/guides/: OK
)
if exist "docs\tutorials\" (
    echo    docs/tutorials/: OK
)

REM 5. Check Source code (for doc reference)
echo [5] Source code locations:
if exist "Source\UnrealCV\" (
    echo    Source/UnrealCV/: OK
) else (
    echo    Source/UnrealCV/: MISSING!
)

REM 6. Show current git status
echo [6] Git status:
git status --short 2>nul || echo    Not a git repo

REM 7. Recent commits
echo [7] Recent commits:
git log --oneline -5 2>nul || echo    No git history

echo ============================================
echo Environment check complete!
echo Next: Read claude-progress.txt
echo ============================================
