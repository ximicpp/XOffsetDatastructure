param(
 [Parameter(Mandatory=$true)][ValidateSet('Plan','Guard')][string]$Mode,
 [Parameter(Mandatory=$true)][string]$PlanFile,
 [Parameter(Mandatory=$true)][string]$HelperSha256,
 [string]$StateDirectory, [string]$PlanSha256, [string]$RunToken,
 [ValidateRange(1,780)][int]$MaximumSeconds=780
)
$ErrorActionPreference='Stop'
$helperBytes=[IO.File]::ReadAllBytes($PSCommandPath)
$helperHasher=[Security.Cryptography.SHA256]::Create()
try {$helperActual=([BitConverter]::ToString($helperHasher.ComputeHash($helperBytes))).Replace('-','').ToLowerInvariant()}
finally {$helperHasher.Dispose()}
if ($helperActual -cne $HelperSha256) { throw 'Helper identity changed' }
$allowed=@('client_ue4-Win64-Shipping','TGitCache')
function Publish-Json([string]$path,$value) {
 $temporary=$path+'.publishing'
 $stream=[IO.File]::Open($temporary,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write)
 try {$writer=[IO.StreamWriter]::new($stream,[Text.UTF8Encoding]::new($false));$writer.WriteLine(($value|ConvertTo-Json -Depth 12));$writer.Flush()}
 finally {$stream.Dispose()}
 [IO.File]::Move($temporary,$path)
}
Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Security.Principal;
using System.Text;
public static class CursorPauseNative {
 [StructLayout(LayoutKind.Sequential)] public struct FT { public uint Low,High; }
 [DllImport("kernel32.dll",SetLastError=true)] public static extern IntPtr OpenProcess(uint access,bool inherit,uint pid);
 [DllImport("kernel32.dll",SetLastError=true)] public static extern bool CloseHandle(IntPtr handle);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetProcessTimes(IntPtr h,out FT created,out FT exited,out FT kernel,out FT user);
 [DllImport("kernel32.dll",SetLastError=true)] public static extern bool GetExitCodeProcess(IntPtr h,out uint code);
 [DllImport("kernel32.dll",SetLastError=true)] public static extern uint WaitForSingleObject(IntPtr h,uint milliseconds);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool QueryFullProcessImageName(IntPtr h,uint flags,StringBuilder path,ref uint count);
 [DllImport("advapi32.dll",SetLastError=true)] static extern bool OpenProcessToken(IntPtr h,uint access,out IntPtr token);
 [DllImport("advapi32.dll",SetLastError=true)] static extern bool GetTokenInformation(IntPtr token,int kind,IntPtr buffer,int bytes,out int required);
 [DllImport("ntdll.dll")] public static extern int NtSuspendProcess(IntPtr handle);
 [DllImport("ntdll.dll")] public static extern int NtResumeProcess(IntPtr handle);
 public sealed class Identity {
   public uint pid; public string name,image_path,owner_sid,creation_filetime,start_time_utc;
 }
 public static IntPtr Open(uint pid,bool suspend) {
   IntPtr h=OpenProcess(0x1000u|0x100000u|(suspend?0x800u:0u),false,pid);
   if(h==IntPtr.Zero)throw new Win32Exception(Marshal.GetLastWin32Error());return h;
 }
 public static Identity Read(IntPtr h,uint pid) {
   uint wait=WaitForSingleObject(h,0);if(wait!=258)throw new InvalidOperationException("Target exited or wait failed");
   uint code; if(!GetExitCodeProcess(h,out code))throw new Win32Exception(Marshal.GetLastWin32Error());
   if(code!=259)throw new InvalidOperationException("Target exited");
   FT c,e,k,u;if(!GetProcessTimes(h,out c,out e,out k,out u))throw new Win32Exception(Marshal.GetLastWin32Error());
   ulong ticks=((ulong)c.High<<32)|c.Low;var path=new StringBuilder(32768);uint count=(uint)path.Capacity;
   if(!QueryFullProcessImageName(h,0,path,ref count))throw new Win32Exception(Marshal.GetLastWin32Error());
   IntPtr token;if(!OpenProcessToken(h,8,out token))throw new Win32Exception(Marshal.GetLastWin32Error());
   string sid;try {
     int required;GetTokenInformation(token,1,IntPtr.Zero,0,out required);
     if(required<=0)throw new Win32Exception(Marshal.GetLastWin32Error());
     IntPtr memory=Marshal.AllocHGlobal(required);try {
       if(!GetTokenInformation(token,1,memory,required,out required))throw new Win32Exception(Marshal.GetLastWin32Error());
       sid=new SecurityIdentifier(Marshal.ReadIntPtr(memory)).Value;
     } finally {Marshal.FreeHGlobal(memory);}
   } finally {CloseHandle(token);}
   return new Identity {pid=pid,name=System.IO.Path.GetFileNameWithoutExtension(path.ToString()),image_path=path.ToString(),owner_sid=sid,
     creation_filetime=ticks.ToString(System.Globalization.CultureInfo.InvariantCulture),
     start_time_utc=DateTime.FromFileTimeUtc((long)ticks).ToString("o",System.Globalization.CultureInfo.InvariantCulture)};
 }
}
'@
$currentSid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
function Allowed-Identity($entry) {
 if ($allowed -notcontains [string]$entry.name) {throw 'Target name is outside the two authorized names'}
 if (-not ([IO.Path]::GetExtension([string]$entry.image_path) -ieq '.exe')) {throw 'Expected executable image'}
 if (-not ([IO.Path]::GetFileNameWithoutExtension([string]$entry.image_path) -ieq [string]$entry.name)) {throw 'Image/name mismatch'}
 if ([string]$entry.owner_sid -cne $currentSid) {throw 'Refusing another Windows owner'}
 if ([string]$entry.creation_filetime -notmatch '^[0-9]{1,20}$') {throw 'Creation FILETIME must be decimal string'}
}
if ($Mode -eq 'Plan') {
 if (Test-Path -LiteralPath $PlanFile) {throw 'Read-only plan output must be fresh'}
 $targets=@()
 foreach($name in $allowed) {
   foreach($process in @(Get-Process -Name $name -ErrorAction SilentlyContinue)) {
     $handle=[CursorPauseNative]::Open([uint32]$process.Id,$false)
     try {$entry=[CursorPauseNative]::Read($handle,[uint32]$process.Id);Allowed-Identity $entry;$targets+=,$entry}
     finally {[void][CursorPauseNative]::CloseHandle($handle)}
   }
 }
 Publish-Json $PlanFile ([ordered]@{schema_version=1;kind='cursor-process-pause-plan-v1';read_only=$true;
   allowed_names=$allowed;controller_owner_sid=$currentSid;created_utc=[DateTimeOffset]::UtcNow.ToString('o');
   targets=@($targets|Sort-Object pid);helper_sha256=$HelperSha256;processes_suspended=0})
 [Console]::WriteLine($PlanFile);exit 0
}
if (-not $StateDirectory -or $RunToken -notmatch '^[0-9a-f]{32}$') {throw 'Fresh state/run token required'}
$ready=Join-Path $StateDirectory 'ready.json';$completed=Join-Path $StateDirectory 'completed.json';$stop=Join-Path $StateDirectory 'stop.json'
$records=[Collections.Generic.List[object]]::new();$errorText=$null;$reason='setup_failure';$pauseUtc=$null;$resumeUtc=$null
$clock=[Diagnostics.Stopwatch]::StartNew();$started=[DateTimeOffset]::UtcNow.ToString('o')
try {
 $bytes=[IO.File]::ReadAllBytes($PlanFile);$sha=[Security.Cryptography.SHA256]::Create()
 try {$actual=([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-','').ToLowerInvariant()} finally {$sha.Dispose()}
 if($actual -cne $PlanSha256){throw 'Plan byte identity changed'}
 $plan=[Text.Encoding]::UTF8.GetString($bytes)|ConvertFrom-Json
 if($plan.schema_version -ne 1 -or $plan.kind -cne 'cursor-process-pause-plan-v1' -or $plan.read_only -ne $true -or
    $plan.controller_owner_sid -cne $currentSid -or $plan.helper_sha256 -cne $HelperSha256){throw 'Plan identity/owner mismatch'}
 $planNames=(@($plan.allowed_names)|Sort-Object)-join ','
 $expectedNames=($allowed|Sort-Object)-join ','
 if($planNames -cne $expectedNames){throw 'Plan name scope changed'}
 $seen=@{};if(@($plan.targets).Count -gt 16){throw 'Target count exceeds bounded plan'}
 # Open and verify ALL durable handles before suspending any target.
 foreach($expected in @($plan.targets)) {
   if($expected.pid -isnot [int] -and $expected.pid -isnot [long]){throw 'PID must be integer'}
   if($expected.pid -le 0 -or $expected.pid -eq $PID -or $seen.ContainsKey([string]$expected.pid)){throw 'PID identity/duplicate/self invalid'}
   $seen[[string]$expected.pid]=$true;Allowed-Identity $expected
   $handle=[CursorPauseNative]::Open([uint32]$expected.pid,$true)
   $record=[ordered]@{handle=$handle;identity=$null;suspended=$false;suspend_status=$null;resume_attempted=$false;resume_status=$null;exited_before_resume=$false;wait_before_resume=$null;wait_after_resume=$null}
   $records.Add($record)
   $actual=[CursorPauseNative]::Read($handle,[uint32]$expected.pid);Allowed-Identity $actual
   foreach($field in @('pid','name','image_path','owner_sid','creation_filetime','start_time_utc')) {
     if([string]$actual.$field -cne [string]$expected.$field){throw ('Handle identity changed: '+$field)}
   }
   $record.identity=$actual
 }
 foreach($record in $records) {
   if((Test-Path -LiteralPath $stop) -or $clock.Elapsed.TotalSeconds -ge $MaximumSeconds){throw 'Stopped/deadline before complete suspension'}
   $status=[CursorPauseNative]::NtSuspendProcess($record.handle);$record.suspend_status=$status
   if($status -lt 0){throw ('NtSuspendProcess failed: '+$status)}
   $record.suspended=$true
 }
 $pauseUtc=[DateTimeOffset]::UtcNow.ToString('o')
 if($clock.Elapsed.TotalSeconds -ge $MaximumSeconds){throw 'Deadline before ready'}
 Publish-Json $ready ([ordered]@{schema_version=1;status='ready';run_token=$RunToken;pid=$PID;started_utc=$started;
   all_suspended_utc=$pauseUtc;maximum_seconds=$MaximumSeconds;targets=@($records|ForEach-Object {$_.identity});
   actual_suspended=$records.Count;held_process_handles=$records.Count})
 while($true) {
   $remaining=$MaximumSeconds-$clock.Elapsed.TotalSeconds
   if($remaining -le 0){$reason='watchdog_deadline';break}
   if(Test-Path -LiteralPath $stop) {
     $request=Get-Content -LiteralPath $stop -Raw|ConvertFrom-Json
     if($request.run_token -cne $RunToken -or $request.stop_requested -ne $true){throw 'Unexpected stop token'}
     $reason=if($clock.Elapsed.TotalSeconds -ge $MaximumSeconds){'watchdog_deadline'}else{'requested_stop'};break
   }
   Start-Sleep -Milliseconds ([int][Math]::Max(1,[Math]::Min(200,[Math]::Floor($remaining*1000))))
 }
} catch {
 $errorText=$_.Exception.Message
 if(-not (Test-Path -LiteralPath $ready)) {
   Publish-Json $ready ([ordered]@{schema_version=1;status='failed';run_token=$RunToken;error=$errorText;pid=$PID})
 }
} finally {
 if($reason -eq 'requested_stop' -and $clock.Elapsed.TotalSeconds -ge $MaximumSeconds){$reason='watchdog_deadline'}
 $resumeUtc=[DateTimeOffset]::UtcNow.ToString('o');$restored=$true
 for($i=$records.Count-1;$i -ge 0;$i--) {
   $record=$records[$i]
   if($record.suspended -and -not $record.resume_attempted) {
     # Observation must never prevent the one owned resume attempt.
     try {$record.wait_before_resume=[CursorPauseNative]::WaitForSingleObject($record.handle,0)}
     catch {$restored=$false;$errorText+='; before-resume observation: '+$_.Exception.Message}
     $record.resume_attempted=$true
     try {
       $record.resume_status=[CursorPauseNative]::NtResumeProcess($record.handle)
     } catch {$restored=$false;$errorText+='; resume: '+$_.Exception.Message}
     try {$record.wait_after_resume=[CursorPauseNative]::WaitForSingleObject($record.handle,0)}
     catch {$restored=$false;$errorText+='; after-resume observation: '+$_.Exception.Message}
     $record.exited_before_resume=($record.wait_before_resume -eq 0 -or $record.wait_after_resume -eq 0)
     if($record.exited_before_resume){$errorText+='; target exited externally around resume'}
     if(($record.wait_before_resume -ne 0 -and $record.wait_before_resume -ne 258) -or
        ($record.wait_after_resume -ne 0 -and $record.wait_after_resume -ne 258)){
       $restored=$false;$errorText+='; process wait failed'
     }
     if($null -ne $record.resume_status -and $record.resume_status -lt 0 -and -not $record.exited_before_resume){$restored=$false;$errorText+='; NtResumeProcess failed'}
   }
 }
 $summary=@($records|ForEach-Object {[ordered]@{identity=$_.identity;suspended=$_.suspended;suspend_status=$_.suspend_status;
   resume_attempted=$_.resume_attempted;resume_status=$_.resume_status;exited_before_resume=$_.exited_before_resume;
   wait_before_resume=$_.wait_before_resume;wait_after_resume=$_.wait_after_resume}})
 foreach($record in $records){[void][CursorPauseNative]::CloseHandle($record.handle)}
 if($reason -eq 'requested_stop' -and $clock.Elapsed.TotalSeconds -ge $MaximumSeconds){$reason='watchdog_deadline'}
 $status=if($restored -and -not $errorText -and $reason -eq 'requested_stop'){'passed'}else{'failed'}
 Publish-Json $completed ([ordered]@{schema_version=1;status=$status;restored=$restored;run_token=$RunToken;reason=$reason;error=$errorText;
   started_utc=$started;all_suspended_utc=$pauseUtc;resume_started_utc=$resumeUtc;finished_utc=[DateTimeOffset]::UtcNow.ToString('o');
   maximum_seconds=$MaximumSeconds;targets=$summary;pid=$PID;no_kill_restart_priority_change=$true})
}
