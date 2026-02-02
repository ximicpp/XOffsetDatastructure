#!/bin/bash

# Continuous CI Monitor - Windows/WSL Compatible
# Runs in background and reports CI status changes

REPO="ximicpp/XOffsetDatastructure"
BRANCH="${1:-next_cpp26}"
LOG_FILE="${2:-ci-monitor.log}"

echo "🔍 Starting CI Monitor for $REPO ($BRANCH)" | tee "$LOG_FILE"
echo "📝 Log file: $LOG_FILE" | tee -a "$LOG_FILE"
echo "⏰ Started at: $(date)" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"

last_run_id=""
last_status=""
last_check_time=""
check_count=0

while true; do
    check_count=$((check_count + 1))
    current_time=$(date "+%Y-%m-%d %H:%M:%S")
    
    # Fetch latest workflow run
    response=$(curl -s "https://api.github.com/repos/$REPO/actions/runs?branch=$BRANCH&per_page=1" 2>/dev/null)
    
    if echo "$response" | grep -q "workflow_runs"; then
        run_id=$(echo "$response" | grep -o '"id": [0-9]*' | head -1 | grep -o '[0-9]*')
        status=$(echo "$response" | grep -o '"status": "[^"]*"' | head -1 | cut -d'"' -f4)
        conclusion=$(echo "$response" | grep -o '"conclusion": "[^"]*"' | head -1 | cut -d'"' -f4)
        run_number=$(echo "$response" | grep -o '"run_number": [0-9]*' | head -1 | grep -o '[0-9]*')
        html_url=$(echo "$response" | grep -o '"html_url": "[^"]*"' | head -1 | cut -d'"' -f4)
        created_at=$(echo "$response" | grep -o '"created_at": "[^"]*"' | head -1 | cut -d'"' -f4)
        
        if [ -n "$run_id" ]; then
            # Report if this is a new run or status changed
            if [ "$run_id" != "$last_run_id" ]; then
                echo "" | tee -a "$LOG_FILE"
                echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" | tee -a "$LOG_FILE"
                echo "🆕 NEW CI RUN DETECTED!" | tee -a "$LOG_FILE"
                echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" | tee -a "$LOG_FILE"
                echo "📌 Run #$run_number (ID: $run_id)" | tee -a "$LOG_FILE"
                echo "🕐 Created: $created_at" | tee -a "$LOG_FILE"
                echo "📊 Status: $status" | tee -a "$LOG_FILE"
                echo "🔗 URL: $html_url" | tee -a "$LOG_FILE"
                echo "" | tee -a "$LOG_FILE"
                
                last_run_id="$run_id"
                last_status="$status"
            elif [ "$status" != "$last_status" ]; then
                echo "" | tee -a "$LOG_FILE"
                echo "[$current_time] 📝 Status changed: $last_status → $status" | tee -a "$LOG_FILE"
                last_status="$status"
                
                # Show more details on status changes
                if [ "$status" = "in_progress" ]; then
                    echo "   ⚙️  Build is now running..." | tee -a "$LOG_FILE"
                elif [ "$status" = "queued" ]; then
                    echo "   ⏳ Build is queued..." | tee -a "$LOG_FILE"
                fi
            fi
            
            # Check if workflow completed
            if [ "$status" = "completed" ] && [ -n "$conclusion" ]; then
                echo "" | tee -a "$LOG_FILE"
                echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" | tee -a "$LOG_FILE"
                echo "🏁 CI WORKFLOW COMPLETED!" | tee -a "$LOG_FILE"
                echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" | tee -a "$LOG_FILE"
                echo "📌 Run #$run_number" | tee -a "$LOG_FILE"
                echo "📊 Conclusion: $conclusion" | tee -a "$LOG_FILE"
                echo "🔗 URL: $html_url" | tee -a "$LOG_FILE"
                echo "" | tee -a "$LOG_FILE"
                
                if [ "$conclusion" = "success" ]; then
                    echo "✅ SUCCESS! All tests passed!" | tee -a "$LOG_FILE"
                    echo "" | tee -a "$LOG_FILE"
                    echo "📋 Next steps:" | tee -a "$LOG_FILE"
                    echo "   1. Review results: $html_url" | tee -a "$LOG_FILE"
                    echo "   2. Archive proposal: openspec archive add-ci-cd-docker-support --yes" | tee -a "$LOG_FILE"
                    exit 0
                elif [ "$conclusion" = "failure" ]; then
                    echo "❌ FAILURE! Some tests failed" | tee -a "$LOG_FILE"
                    echo "   Check logs: $html_url" | tee -a "$LOG_FILE"
                    
                    # Reset to monitor for next run (in case of fixes)
                    echo "" | tee -a "$LOG_FILE"
                    echo "⏳ Waiting for new runs (will continue monitoring)..." | tee -a "$LOG_FILE"
                    last_run_id=""  # Reset to catch the next run
                else
                    echo "⚠️  Completed with status: $conclusion" | tee -a "$LOG_FILE"
                    echo "   Review: $html_url" | tee -a "$LOG_FILE"
                fi
            fi
        else
            if [ $check_count -eq 1 ]; then
                echo "[$current_time] ℹ️  No CI runs found yet for branch '$BRANCH'" | tee -a "$LOG_FILE"
                echo "   Waiting for CI to start..." | tee -a "$LOG_FILE"
            fi
        fi
    else
        if [ $check_count -eq 1 ]; then
            echo "[$current_time] ⚠️  Could not fetch CI status" | tee -a "$LOG_FILE"
        fi
    fi
    
    # Periodic heartbeat (every 10 checks)
    if [ $((check_count % 10)) -eq 0 ]; then
        echo "[$current_time] 💓 Heartbeat #$check_count - Monitoring..." >> "$LOG_FILE"
    fi
    
    # Wait 30 seconds before next check
    sleep 30
done
