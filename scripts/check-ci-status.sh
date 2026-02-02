#!/bin/bash

# GitHub Actions CI Status Checker
# Periodically checks the CI status for the current branch

set -e

REPO="ximicpp/XOffsetDatastructure"
BRANCH="${1:-next_cpp26}"
CHECK_INTERVAL="${2:-60}" # seconds
MAX_CHECKS="${3:-120}"     # max checks (120 * 60s = 2 hours)

echo "=========================================="
echo "  GitHub Actions CI Status Checker"
echo "=========================================="
echo "Repository: $REPO"
echo "Branch: $BRANCH"
echo "Check interval: ${CHECK_INTERVAL}s"
echo "Max duration: $((MAX_CHECKS * CHECK_INTERVAL / 60)) minutes"
echo "=========================================="
echo ""

check_count=0
last_run_id=""
last_status=""

while [ $check_count -lt $MAX_CHECKS ]; do
    check_count=$((check_count + 1))
    current_time=$(date "+%Y-%m-%d %H:%M:%S")
    
    echo "[$current_time] Check #$check_count/$MAX_CHECKS"
    
    # Fetch latest workflow run
    response=$(curl -s "https://api.github.com/repos/$REPO/actions/runs?branch=$BRANCH&per_page=1")
    
    # Check if we got valid response
    if echo "$response" | grep -q "workflow_runs"; then
        run_id=$(echo "$response" | grep -o '"id": [0-9]*' | head -1 | grep -o '[0-9]*')
        status=$(echo "$response" | grep -o '"status": "[^"]*"' | head -1 | cut -d'"' -f4)
        conclusion=$(echo "$response" | grep -o '"conclusion": "[^"]*"' | head -1 | cut -d'"' -f4)
        workflow_name=$(echo "$response" | grep -o '"name": "[^"]*"' | head -1 | cut -d'"' -f4)
        run_number=$(echo "$response" | grep -o '"run_number": [0-9]*' | head -1 | grep -o '[0-9]*')
        html_url=$(echo "$response" | grep -o '"html_url": "[^"]*"' | head -1 | cut -d'"' -f4)
        
        if [ -n "$run_id" ]; then
            # New run detected or status changed
            if [ "$run_id" != "$last_run_id" ] || [ "$status" != "$last_status" ]; then
                echo "  ┌─────────────────────────────────────"
                echo "  │ Workflow: $workflow_name"
                echo "  │ Run #$run_number (ID: $run_id)"
                echo "  │ Status: $status"
                [ -n "$conclusion" ] && echo "  │ Conclusion: $conclusion"
                echo "  │ URL: $html_url"
                echo "  └─────────────────────────────────────"
                
                last_run_id="$run_id"
                last_status="$status"
            else
                echo "  Status: $status (unchanged)"
            fi
            
            # Check if workflow completed
            if [ "$status" = "completed" ]; then
                echo ""
                echo "=========================================="
                echo "  CI WORKFLOW COMPLETED"
                echo "=========================================="
                echo "Conclusion: $conclusion"
                echo "URL: $html_url"
                echo ""
                
                if [ "$conclusion" = "success" ]; then
                    echo "✓ SUCCESS: All tests passed!"
                    echo ""
                    echo "Next steps:"
                    echo "1. Review the workflow results"
                    echo "2. Archive the proposal:"
                    echo "   openspec archive add-ci-cd-docker-support --yes"
                    exit 0
                elif [ "$conclusion" = "failure" ]; then
                    echo "✗ FAILURE: Some tests failed"
                    echo ""
                    echo "Please check the workflow logs for details:"
                    echo "$html_url"
                    exit 1
                else
                    echo "⚠ COMPLETED WITH STATUS: $conclusion"
                    echo "Please review: $html_url"
                    exit 2
                fi
            fi
        else
            echo "  No workflow runs found for branch '$BRANCH'"
            echo "  (CI may not have started yet)"
        fi
    else
        echo "  ⚠ Failed to fetch CI status (API rate limit or network issue)"
    fi
    
    # Wait before next check (unless it's the last check)
    if [ $check_count -lt $MAX_CHECKS ]; then
        echo "  Waiting ${CHECK_INTERVAL}s before next check..."
        echo ""
        sleep $CHECK_INTERVAL
    fi
done

echo ""
echo "=========================================="
echo "  TIMEOUT"
echo "=========================================="
echo "Maximum checks reached without completion"
echo "Latest status: $last_status"
echo ""
echo "Check manually at:"
echo "https://github.com/$REPO/actions?query=branch%3A$BRANCH"
exit 3
