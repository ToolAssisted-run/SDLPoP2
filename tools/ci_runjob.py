#!/usr/bin/env python3
"""ci_runjob.py WORKFLOW JOB > script.sh (tools/ci-rehearse.sh): the job's `run:` steps as one bash script, emulating GitHub's step environment"""
import sys, yaml
wf = yaml.safe_load(open(sys.argv[1])); job = wf['jobs'][sys.argv[2]]
out = ['#!/bin/bash', 'set -e', 'export CI=true GITHUB_ENV=/tmp/gh_env GITHUB_PATH=/tmp/gh_path GITHUB_OUTPUT=/tmp/gh_out',
       ': > $GITHUB_ENV; : > $GITHUB_PATH', 'sudo() { "$@"; }   # (containers run as root)', 'export -f sudo', 'cd /work']
for i, st in enumerate(job['steps']):
    if 'run' not in st: out.append('echo "::: skipped (action) %s"' % st.get('uses', st.get('name'))); continue
    out.append('echo "::: step %d: %s"' % (i, st.get('name', '')))
    out.append('while read -r p; do [ -n "$p" ] && export PATH="$p:$PATH"; done < $GITHUB_PATH')
    out.append('set -a; . $GITHUB_ENV; set +a')
    out.append('( set -e\n' + st['run'] + '\n)')
print('\n'.join(out))
