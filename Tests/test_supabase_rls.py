"""Run SQL/RLS integration on a disposable local PostgreSQL, never a remote project."""
import argparse
from pathlib import Path
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--bin', required=True, type=Path)
parser.add_argument('--share', required=True, type=Path)
args = parser.parse_args()

def run(command):
    result = subprocess.run([str(x) for x in command], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60)
    if result.returncode:
        raise RuntimeError(result.stdout)
    return result.stdout

with tempfile.TemporaryDirectory(prefix='jaspro-pg-') as temporary:
    root = Path(temporary)
    data = root / 'data'
    ctl = args.bin / 'pg_ctl'
    run([args.bin / 'initdb', '-D', data, '-U', 'jaspro_test', '-L', args.share, '--auth-local=trust', '--auth-host=reject', '--encoding=UTF8', '--locale=C', '--no-instructions'])
    started = False
    try:
        options = shlex.join(['-F', '-k', str(root), '-h', '', '-p', '55433', '-c', 'unix_socket_permissions=0700', '-c', 'max_connections=10', '-c', 'shared_buffers=16MB'])
        run([ctl, '-D', data, '-l', root / 'server.log', '-o', options, '-w', 'start'])
        started = True
        print(run([args.bin / 'psql', '-h', root, '-p', '55433', '-d', 'postgres', '-U', 'jaspro_test', '-X', '-v', 'ON_ERROR_STOP=1', '-f', Path(__file__).with_name('supabase_rls.sql')]))
    finally:
        if started:
            run([ctl, '-D', data, '-m', 'fast', '-w', 'stop'])
