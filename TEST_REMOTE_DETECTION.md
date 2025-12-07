# Release Script Remote Detection Testing

## Overview

The improved `release.sh` script now intelligently detects the appropriate git remote to use for pushing release tags. This document describes the improvements and how to test them.

## Improvements Made

### 1. **Dynamic Remote Detection**

Previously, the script hard-coded the remote name as `github`:
```bash
git push github "$RELEASE_TAG"  # Hard-coded, fails if remote has different name
```

Now, the script uses a helper function `get_github_remote()` to automatically detect the appropriate remote:
```bash
GITHUB_REMOTE=$(get_github_remote)
git push "$GITHUB_REMOTE" "$RELEASE_TAG"  # Dynamic, works with any remote name
```

### 2. **Smart Priority System**

The `get_github_remote()` function implements a priority-based selection:

1. **Primary**: Remotes with `github.com` in the URL (e.g., `git@github.com:...`)
   - Searches all configured remotes
   - Selects the first one matching this pattern
   - Ensures GitHub remotes are preferred

2. **Fallback**: First available remote
   - If no github.com remotes exist
   - Uses the first remote in the list

3. **Error**: No remotes found
   - Script exits with error message
   - Prevents unexpected behavior

### 3. **Transparent Remote Display**

The script now displays the selected remote at the start:

```
Using remote: github (git@github.com:wysaid/CameraCapture.git)
```

This allows users to verify the correct remote is being used before proceeding.

## Test Scenarios

### Scenario 1: Standard Setup (github.com + Local Upstream)

**Configuration:**
```bash
github    git@github.com:wysaid/CameraCapture.git (fetch)
github    git@github.com:wysaid/CameraCapture.git (push)
origin    git@git.corp.kuaishou.com:facemagic/ccap.git (fetch)
origin    git@git.corp.kuaishou.com:facemagic/ccap.git (push)
```

**Expected Behavior:**
- Script detects both remotes
- Prioritizes `github` (contains github.com)
- Displays: `Using remote: github (git@github.com:wysaid/CameraCapture.git)`
- Executes: `git push github v1.4.0`

**Verification:**
```bash
./scripts/release.sh --dry-run
# Look for: "Using remote: github" in output
```

### Scenario 2: Non-standard Remote Names

**Configuration:**
```bash
upstream    git@github.com:wysaid/CameraCapture.git (fetch)
upstream    git@github.com:wysaid/CameraCapture.git (push)
internal    git@git.corp.kuaishou.com:facemagic/ccap.git (fetch)
internal    git@git.corp.kuaishou.com:facemagic/ccap.git (push)
```

**Expected Behavior:**
- Script detects `upstream` as GitHub remote (even though it's not named `github`)
- Displays: `Using remote: upstream (git@github.com:wysaid/CameraCapture.git)`
- Executes: `git push upstream v1.4.0`

**Why This Matters:**
- Developers with different remote naming conventions work seamlessly
- Corporate environments with multiple remotes are supported
- Fork/upstream patterns are automatically handled

### Scenario 3: Only Local Upstream

**Configuration:**
```bash
origin    git@git.corp.kuaishou.com:facemagic/ccap.git (fetch)
origin    git@git.corp.kuaishou.com:facemagic/ccap.git (push)
```

**Expected Behavior:**
- No github.com remote found
- Script falls back to first available: `origin`
- Displays: `Using remote: origin (git@git.corp.kuaishou.com:facemagic/ccap.git)`
- Executes: `git push origin v1.4.0`

**Note:** This works but isn't ideal for public releases to GitHub.

### Scenario 4: No Remotes

**Configuration:**
```bash
(no remotes configured)
```

**Expected Behavior:**
- Script exits with error: `Error: No git remote found!`
- Prevents accidental release creation
- User must configure at least one remote

## Implementation Details

### Helper Function: `get_github_remote()`

Located at the beginning of the script (lines 38-57):

```bash
get_github_remote() {
    # First, try to find a remote with github.com in the URL
    local github_remote=$(git remote -v | grep 'github\.com' | awk '{print $1}' | head -1)
    
    if [ -n "$github_remote" ]; then
        echo "$github_remote"
        return 0
    fi
    
    # Fallback: return first available remote
    local first_remote=$(git remote | head -1)
    if [ -n "$first_remote" ]; then
        echo "$first_remote"
        return 0
    fi
    
    # No remotes found
    return 1
}
```

### Usage in Script

1. **Initialization (line 60):**
   ```bash
   GITHUB_REMOTE=$(get_github_remote)
   ```

2. **Validation (line 61-63):**
   ```bash
   if [ -z "$GITHUB_REMOTE" ]; then
       exit 1
   fi
   ```

3. **Display (line 70):**
   ```bash
   echo -e "  Using remote: ${GREEN}$GITHUB_REMOTE${NC} ($(git remote get-url "$GITHUB_REMOTE"))"
   ```

4. **Dry-run Command (line 314):**
   ```bash
   echo -e "  ${BLUE}git push $GITHUB_REMOTE $RELEASE_TAG${NC}"
   ```

5. **Actual Push (line 327):**
   ```bash
   git push "$GITHUB_REMOTE" "$RELEASE_TAG"
   ```

## Benefits

✅ **Flexibility**: Works with any remote naming convention  
✅ **Intelligence**: Automatically prioritizes GitHub remotes  
✅ **Safety**: Prevents silent failures with unsupported remote configurations  
✅ **Transparency**: Users see exactly which remote will be used  
✅ **Robustness**: Graceful fallback for edge cases  
✅ **Maintenance**: Script requires no manual configuration  

## Migration Guide

### For Existing Users

**No changes required!** The script is fully backward compatible.

If your project uses a remote named `github` pointing to github.com:
- Script will detect and use it automatically
- Behavior is identical to previous version

If your project uses a different remote name:
- Script will now work correctly instead of failing
- No additional configuration needed

### For New Projects

Simply configure your remotes normally:
```bash
git remote add github https://github.com/yourusername/yourrepo.git
git remote add origin https://internal-git.company.com/yourrepo.git
```

The script handles the rest automatically.

## Testing the Changes

### Quick Test: Dry-run Mode

```bash
# Test without actually creating tags
./scripts/release.sh --dry-run

# Expected output includes:
# "Using remote: github (git@github.com:wysaid/CameraCapture.git)"
# "git push github v1.4.0"  (or whichever remote is detected)
```

### Manual Test: Check Remote Detection

```bash
# Run the detection function in isolation
bash -c '
SCRIPT_DIR="/path/to/scripts"
cd "$(dirname "$SCRIPT_DIR")"

get_github_remote() {
    local github_remote=$(git remote -v | grep "github\.com" | awk "{print \$1}" | head -1)
    [ -n "$github_remote" ] && echo "$github_remote" && return 0
    
    local first_remote=$(git remote | head -1)
    [ -n "$first_remote" ] && echo "$first_remote" && return 0
    
    return 1
}

echo "Detected remote: $(get_github_remote)"
'
```

### Full Integration Test

```bash
# With version 1.4.0 prepared but not yet released:
./scripts/release.sh --dry-run

# Verify:
# 1. Correct remote is shown
# 2. All 7 validation steps pass
# 3. Correct git commands would be executed
# 4. Script exits with success
```

## Related Files

- `scripts/release.sh` - Main release script (improved)
- `RELEASING.md` - Release procedure documentation
- `scripts/update_version.sh` - Version update script

## See Also

- GitHub PR #25: Version Management System Implementation
- PR Review Comments: Git remote handling improvements
