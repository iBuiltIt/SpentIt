"""POSIX synthetic CLI checks; invoked with the transport-test and reader binaries."""
import subprocess, os, resource, signal, tempfile, json, sys
server=subprocess.Popen([sys.argv[1], '--child'],stdout=subprocess.PIPE,text=True)
try:
 endpoint=server.stdout.readline().strip();assert endpoint
 cli=[sys.argv[2], endpoint]
 before=subprocess.run(cli+['status'],capture_output=True,text=True,check=True)
 def limit():
  resource.setrlimit(resource.RLIMIT_FSIZE,(0,0));signal.signal(signal.SIGXFSZ,signal.SIG_IGN)
 with tempfile.TemporaryFile() as output:
  failed=subprocess.run(cli+['status'],stdout=output,preexec_fn=limit)
 assert failed.returncode==5,failed.returncode
 after=subprocess.run(cli+['status'],capture_output=True,text=True,check=True)
 assert json.loads(before.stdout)['latest']==json.loads(after.stdout)['latest']
 timeout=subprocess.run(cli+['wait-operation','999','100'],capture_output=True,text=True)
 assert timeout.returncode==4,timeout.returncode
 print('output failure exit=5; unchanged outcome; bounded wait timeout exit=4')
finally:
 server.terminate();server.wait()
 import shutil
 if 'endpoint' in locals() and os.path.basename(os.path.dirname(endpoint)).startswith('spentit-observe-'):
  shutil.rmtree(os.path.dirname(endpoint),ignore_errors=True)
