#!/bin/bash
# ci-rehearse.sh [JOB...]: run the CI jobs of .github/workflows/build.yml locally in Docker, on a clean clone of the
# committed HEAD (uncommitted changes are not included), in the job's own Ubuntu image. The `run:` steps come from
# the workflow itself (tools/ci_runjob.py); actions (checkout, upload) are skipped. Packages land in ./ci-rehearse/.
# Needs docker and python3-yaml. Default jobs: linux windows.
set -e
root=$(cd "$(dirname "$0")/.." && pwd); out=$PWD/ci-rehearse; mkdir -p "$out"
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
git clone -q "$root" "$tmp/repo"
cat > "$tmp/prelude.sh" <<'P'
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq && apt-get install -y -qq --no-install-recommends git ca-certificates curl zip python3 > /dev/null
git config --global --add safe.directory /work
P
for job in ${@:-linux windows}; do
	image=$(python3 -c "import yaml,sys; print(yaml.safe_load(open(sys.argv[1]))['jobs'][sys.argv[2]]['runs-on'].replace('ubuntu-latest','ubuntu-24.04').replace('-',':'))" "$tmp/repo/.github/workflows/build.yml" "$job")
	python3 "$root/tools/ci_runjob.py" "$tmp/repo/.github/workflows/build.yml" "$job" > "$tmp/$job.sh"
	echo "=== $job ($image)"
	docker run --rm -v "$tmp/repo:/src:ro" -v "$tmp:/ci" -v "$out:/out" "$image" \
		bash -c "bash /ci/prelude.sh && cp -r /src /work && DEBIAN_FRONTEND=noninteractive bash /ci/$job.sh && cp /work/dist/*.* /out/" \
		> "$out/$job.log" 2>&1 && echo "=== $job: passed (log: $out/$job.log)" || { echo "=== $job: FAILED (log: $out/$job.log)"; exit 1; }
done
ls -la "$out"
