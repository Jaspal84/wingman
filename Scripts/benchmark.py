#!/usr/bin/env python3
"""Measure one explicitly provided command. Report this host, never infer M1 speed."""
import argparse
import json
import os
import platform
import resource
import subprocess
import time
from pathlib import Path

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('command',nargs=argparse.REMAINDER)
args=parser.parse_args()
command=args.command[1:] if args.command[:1]==['--'] else args.command
if not command: parser.error('A command is required after --')
start=time.perf_counter();before=resource.getrusage(resource.RUSAGE_CHILDREN)
result=subprocess.run(command,check=False)
usage=resource.getrusage(resource.RUSAGE_CHILDREN)
cpu=platform.processor()
if Path('/proc/cpuinfo').exists():
    for line in Path('/proc/cpuinfo').read_text().splitlines():
        if line.startswith('model name'): cpu=line.split(':',1)[1].strip();break
report={'platform':platform.platform(),'architecture':platform.machine(),'cpu':cpu,
        'available_cpus':len(os.sched_getaffinity(0)) if hasattr(os,'sched_getaffinity') else os.cpu_count(),
        'command':command,'exit_code':result.returncode,'wall_seconds':time.perf_counter()-start,
        'cpu_seconds':usage.ru_utime+usage.ru_stime-before.ru_utime-before.ru_stime,
        'peak_rss_bytes':usage.ru_maxrss if platform.system()=='Darwin' else usage.ru_maxrss*1024,
        'method':'Fresh benchmark process; perf_counter wall time and child getrusage. Not an M1 or inference benchmark.'}
args.output.parent.mkdir(parents=True,exist_ok=True)
args.output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
raise SystemExit(result.returncode)
