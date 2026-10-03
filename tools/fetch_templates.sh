#!/usr/bin/env bash
# Downloads the game runtime for other platforms - the "player templates"
# Build Game uses when its Platform is not this machine - from this
# repository's CI: the player/ folder of the latest successful Windows,
# Linux and macOS runs, into ~/.pyros3d/templates/<platform>/.
#
#   tools/fetch_templates.sh [--branch master] [--graphics vulkan|opengl|metal]
#                            [windows] [linux] [macos]
#
# No platform named: all three. Needs the GitHub CLI (gh), signed in.
# Graphics: the backend the downloaded player renders with; the default is
# Vulkan on Windows and Linux and Metal on macOS.
set -euo pipefail

branch=master
graphics=""
platforms=()
while [ $# -gt 0 ]; do
	case "$1" in
		--branch) branch="$2"; shift 2 ;;
		--graphics) graphics="$2"; shift 2 ;;
		windows|linux|macos) platforms+=("$1"); shift ;;
		*) echo "unknown argument: $1" >&2; exit 2 ;;
	esac
done
[ ${#platforms[@]} -eq 0 ] && platforms=(windows linux macos)
command -v gh >/dev/null || { echo "the GitHub CLI (gh) is needed: https://cli.github.com" >&2; exit 1; }

dest="${PYROS_TEMPLATES:-$HOME/.pyros3d/templates}"
for platform in "${platforms[@]}"; do
	case "$platform" in
		windows) workflow=Windows; artifact="Pyros3D-windows-${graphics:-vulkan}-dll" ;;
		linux)   workflow=Linux;   artifact="Pyros3D-linux-${graphics:-vulkan}" ;;
		macos)   workflow=macOS;   artifact="Pyros3D-macos-${graphics:-metal}" ;;
	esac
	run=$(gh run list --workflow "$workflow" --branch "$branch" --status success --limit 1 --json databaseId -q '.[0].databaseId')
	if [ -z "$run" ]; then echo "$platform: no successful $workflow run on $branch" >&2; continue; fi
	tmp=$(mktemp -d)
	echo "$platform: $artifact from run $run"
	if ! gh run download "$run" -n "$artifact" -D "$tmp" 2>/dev/null; then
		echo "$platform: that run has no artifact $artifact (expired, or another --graphics?)" >&2
		rm -rf "$tmp"; continue
	fi
	if [ ! -d "$tmp/player" ]; then
		echo "$platform: the artifact has no player/ folder - it predates player packaging" >&2
		rm -rf "$tmp"; continue
	fi
	rm -rf "$dest/$platform"
	mkdir -p "$dest/$platform"
	cp -R "$tmp/player/." "$dest/$platform/"
	# Artifacts do not keep the executable bit.
	[ "$platform" != windows ] && chmod +x "$dest/$platform/PyrosPlayer" "$dest/$platform/PyrosServer" 2>/dev/null || true
	rm -rf "$tmp"
	echo "$platform: $(ls "$dest/$platform" | wc -l | tr -d ' ') files in $dest/$platform"
done
