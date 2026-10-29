"""macOS native smoke/readiness check; never captures pixels or opens user data."""
import subprocess, socket, json, time, os, sys
p=subprocess.Popen([sys.argv[1], '--diagnostics', '--smoke'],stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,text=True,env={**os.environ,'QT_QPA_PLATFORM':'cocoa'})
endpoint=None
for line in p.stderr:
 if line.startswith('Diagnostics endpoint: '):endpoint=line.strip().split(': ',1)[1];break
assert endpoint
seen=[]
deadline=time.monotonic()+15
while p.poll() is None and time.monotonic()<deadline:
 try:
  with socket.socket(socket.AF_UNIX) as s:
   s.settimeout(.3);s.connect(endpoint);s.sendall(b'{"method":"status"}\n');response=b''
   while not response.endswith(b'\n'):
    chunk=s.recv(65536)
    if not chunk:raise OSError('Disconnected')
    response+=chunk
    if len(response)>4*1024*1024:raise ValueError('Response limit')
  v=json.loads(response);seen.append((v['ui']['native_exposed'],v['ui']['interaction_ready'],v['latest']['presentation']))
 except (OSError,ValueError):pass
 time.sleep(.02)
if p.poll() is None:
 p.kill();p.wait();raise AssertionError('Native smoke timed out')
assert p.wait()==0,p.stderr.read()
print('samples',len(seen),'native_exposed',any(x[0] for x in seen),'presentation_observed',any(x[2]==3 for x in seen))
assert any(x[0] for x in seen) and any(x[2]==3 for x in seen),seen
