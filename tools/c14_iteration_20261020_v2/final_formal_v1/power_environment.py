"""Temporary Windows power scheme with an independent bounded restore watchdog.

Call only while holding the shared WSL measurement/validation lease. State lives
on C: so the hidden Windows watchdog can restore after a killed Linux controller.
The fixed watchdog does not sample performance or alter benchmark pacing.
"""
from pathlib import Path
import base64,hashlib,json,os,subprocess,time

class PowerError(RuntimeError):pass
def require(c,m):
    if not c:raise PowerError(m)
def quote(s):return "'"+str(s).replace("'","''")+"'"
def put(p,v):
    with p.open('x',encoding='utf-8') as f:json.dump(v,f,ensure_ascii=False,indent=2);f.write('\n')
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def file_id(p):return {'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}

class PowerWindow:
    def __init__(self,state_dir,maximum_seconds,helper=None):
        self.state_dir=Path(state_dir).absolute();self.maximum_seconds=maximum_seconds
        self.helper=Path(helper) if helper else Path(__file__).with_name('power-control.ps1')
        require(type(maximum_seconds) is int and 1<=maximum_seconds<=7200,'invalid watchdog duration')
        require(str(self.state_dir).startswith('/mnt/c/') and self.state_dir.resolve()==self.state_dir,
                'power state must be an owned absolute real path on C:')
        self.ready=None;self.completed=None
    def __enter__(self):
        self.state_dir.mkdir(parents=True,exist_ok=False)
        converted=subprocess.run(['/usr/bin/wslpath','-w',str(self.state_dir)],capture_output=True,text=True,check=True,timeout=10).stdout.strip()
        payload='$taskStateDir='+quote(converted)+'; $taskMaximumSeconds='+str(self.maximum_seconds)+';\n'+self.helper.read_text(encoding='utf-8')
        encoded=base64.b64encode(payload.encode('utf-16-le')).decode('ascii')
        launcher="$taskExe=Join-Path $PSHOME 'powershell.exe'; $taskProcess=Start-Process -WindowStyle Hidden -FilePath $taskExe -ArgumentList @('-NoProfile','-NonInteractive','-EncodedCommand',"+quote(encoded)+") -PassThru; [Console]::Write($taskProcess.Id)"
        command=base64.b64encode(launcher.encode('utf-16-le')).decode('ascii')
        exe=Path('/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe')
        powercfg=Path('/mnt/c/Windows/System32/powercfg.exe')
        require(len(command)<28000,'encoded launch command exceeds conservative Windows command bound')
        request={'helper':file_id(self.helper),'module':file_id(Path(__file__)),
            'powershell_executable':file_id(exe),'powercfg_executable':file_id(powercfg),
            'payload_sha256':hashlib.sha256(payload.encode('utf-16-le')).hexdigest(),
            'maximum_seconds':self.maximum_seconds,'state_windows_path':converted,'background_window_style':'Hidden'}
        put(self.state_dir/'request.json',request)
        try:
            # Windows PowerShell may emit localized OEM/CLIXML stderr even when
            # the JSON state files are explicit UTF-8. Keep both streams raw.
            r=subprocess.run([str(exe),'-NoProfile','-NonInteractive','-EncodedCommand',command],capture_output=True,timeout=20)
            (self.state_dir/'launch.stdout').write_bytes(r.stdout)
            (self.state_dir/'launch.stderr').write_bytes(r.stderr)
            put(self.state_dir/'launch.json',{'returncode':r.returncode,
                'stdout':file_id(self.state_dir/'launch.stdout'),'stderr':file_id(self.state_dir/'launch.stderr')})
            require(r.returncode==0 and r.stdout.strip().isdigit(),'hidden Windows restore watchdog failed to launch')
            deadline=time.monotonic()+30
            while not (self.state_dir/'ready.json').exists() and time.monotonic()<deadline:time.sleep(0.1)
            require((self.state_dir/'ready.json').is_file(),'power watchdog readiness deadline expired')
            self.ready=read(self.state_dir/'ready.json')
            require(self.ready.get('status')=='ready','temporary power setup failed: '+str(self.ready))
        except BaseException:
            # __exit__ is not called when __enter__ raises. Explicitly request
            # restoration even if the launcher failed after spawning the helper.
            self._stop()
            raise
        return self
    def _stop(self):
        p=self.state_dir/'stop.json'
        if not p.exists():put(p,{'stop_requested':True,'time_ns':time.time_ns()})
        deadline=time.monotonic()+30
        while not (self.state_dir/'completed.json').exists() and time.monotonic()<deadline:time.sleep(0.1)
        require((self.state_dir/'completed.json').is_file(),'power restore receipt unavailable; watchdog remains bounded')
        self.completed=read(self.state_dir/'completed.json')
        require(self.completed.get('status')=='passed' and self.completed.get('restored') is True,
                'power window restoration/condition failed: '+str(self.completed))
    def __exit__(self,kind,value,tb):
        self._stop()
        return False
