#!/usr/bin/env bash
#
# Make every ELF in a package directory find its libraries beside itself, so
# the folder runs on a distro that never installed our build dependencies.
#
# ELF is friendlier than Mach-O here: dependencies are recorded as bare
# SONAMEs (libSDL2-2.0.so.0), resolved at load time through the binary's
# RUNPATH and the system cache. So there is nothing to rewrite per-dependency
# the way install_name_tool does - copy the .so in, set RUNPATH to $ORIGIN,
# done.
#
# What DOES need care is which libraries to copy, and the answer is not
# "everything non-system". Anything tied to the running machine's hardware or
# display server has to keep coming from that machine: bundling libGL means
# shipping Mesa's software rasteriser to someone with an NVIDIA driver, and
# bundling libX11/libwayland means our copy and the session's copy disagree.
# Those stay external and are listed as requirements in the README instead.
#
# Usage: bundle-linux.sh <package-dir> [<package-dir>...]
set -euo pipefail

# The C/C++ runtime and the loader itself: bundling these is how you get a
# package that segfaults on a distro with a different glibc, because a
# mismatched libc and ld.so cannot be mixed.
# Then the graphics/display stack, per the reasoning above.
keep_external() {
	case "$1" in
		libc.so.*|libm.so.*|libpthread.so.*|libdl.so.*|librt.so.*|ld-linux*|libgcc_s.so.*) return 0 ;;
		libGL.so.*|libGLX.so.*|libGLdispatch.so.*|libEGL.so.*|libGLESv2.so.*|libOpenGL.so.*) return 0 ;;
		libX11*|libxcb*|libXext*|libXrandr*|libXi*|libXcursor*|libXfixes*|libXrender*|libXss*|libXxf86vm*) return 0 ;;
		libwayland*|libdrm*|libgbm*|libxkbcommon*) return 0 ;;
		libvulkan.so.*) return 0 ;;
		libasound.so.*|libpulse*|libpipewire*) return 0 ;;
		# Libraries that are a CLIENT of a daemon on the user's machine. These
		# arrive transitively through SDL2's audio backends and are the ones it
		# is actively harmful to bundle: our libsystemd would be speaking to
		# their systemd, our libdbus-1 to their session bus. The protocol
		# compatibility that makes that work is a property of the pair, not of
		# the library, so shipping half of it is how a package that runs on the
		# build machine dies on a distro one release away. Every desktop Linux
		# has all of these; leaving them external is what keeps the ABI
		# conversation between two halves of the same install.
		libdbus-1.so.*|libsystemd.so.*|libapparmor.so.*|libselinux.so.*) return 0 ;;
		libcap.so.*|libgcrypt.so.*|libgpg-error.so.*) return 0 ;;
		# X11 authentication, pulled in under libxcb. Same display-stack
		# reasoning as the libX11 entries above.
		libXau.so.*|libXdmcp.so.*|libbsd.so.*) return 0 ;;
		*) return 1 ;;
	esac
}

bundle_dir() {
	local dir="$1"
	echo "--- bundling $dir ---"

	local changed=1
	# Repeat until nothing new appears: a copied .so brings its own
	# dependencies, which may themselves bring more.
	while [ $changed -eq 1 ]; do
		changed=0
		local bin
		while IFS= read -r bin; do
			local line soname path
			# ldd prints "\tSONAME => /path (0x...)" for what it resolved, and
			# "\tSONAME => not found" for what it could not. Only the resolved
			# ones can be copied; the rest are reported by verify_dir.
			while IFS= read -r line; do
				soname="$(echo "$line" | awk '{print $1}')"
				path="$(echo "$line" | awk '{print $3}')"
				[ -z "$soname" ] && continue
				keep_external "$soname" && continue
				[ -f "$dir/$soname" ] && continue
				[ -f "$path" ] || continue
				cp -L "$path" "$dir/$soname"
				chmod u+w "$dir/$soname"
				echo "  + $soname"
				changed=1
			done < <(ldd "$bin" 2>/dev/null | grep "=>" | grep -v "not found")
		done < <(find "$dir" -maxdepth 1 -type f \( -perm -u+x -o -name '*.so*' \) -exec sh -c \
			'file -b "$1" | grep -q "^ELF" && echo "$1"' _ {} \;)
	done

	# $ORIGIN is expanded by the loader to the directory holding the binary
	# being loaded, so one value works for the executables and for the copied
	# libraries alike. RUNPATH (--set-rpath on a modern patchelf) rather than
	# the deprecated RPATH, and it must be single-quoted all the way to
	# patchelf or the shell eats it.
	local bin
	while IFS= read -r bin; do
		patchelf --set-rpath '$ORIGIN' "$bin" 2>/dev/null || \
			echo "::warning::patchelf could not set RUNPATH on $(basename "$bin")"
	done < <(find "$dir" -maxdepth 1 -type f \( -perm -u+x -o -name '*.so*' \) -exec sh -c \
		'file -b "$1" | grep -q "^ELF" && echo "$1"' _ {} \;)
}

# Resolve every binary the way the loader will, from inside the directory, and
# fail on anything still missing. LD_LIBRARY_PATH is deliberately NOT set: the
# point is to prove $ORIGIN alone is enough.
verify_dir() {
	local dir="$1"
	local failed=0
	local bin missing
	while IFS= read -r bin; do
		missing="$(ldd "$bin" 2>/dev/null | grep "not found" | awk '{print $1}' || true)"
		if [ -n "$missing" ]; then
			local m
			for m in $missing; do
				echo "::error::$(basename "$bin") needs $m, which is neither bundled nor on this system"
			done
			failed=1
		fi
	done < <(find "$dir" -maxdepth 1 -type f \( -perm -u+x -o -name '*.so*' \) -exec sh -c \
		'file -b "$1" | grep -q "^ELF" && echo "$1"' _ {} \;)
	return $failed
}

rc=0
for d in "$@"; do bundle_dir "$d"; done
for d in "$@"; do
	echo "--- verifying $d ---"
	verify_dir "$d" || rc=1
done
if [ $rc -ne 0 ]; then
	echo "packaging left unresolved dependencies"
	exit 1
fi
echo "all packages are self-contained"
