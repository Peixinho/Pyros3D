#!/usr/bin/env bash
#
# Make every Mach-O in a package directory load its dependencies from beside
# itself, so the folder runs on a Mac that has no Homebrew.
#
# Unlike Windows - where the loader searches the .exe's own directory and
# copying the DLLs in is the whole job - a macOS binary records an ABSOLUTE
# install name per dependency (/opt/homebrew/opt/sdl2-compat/lib/...). Copying
# the dylib next to the executable changes nothing; the path in the load
# command still points at the build machine. So each one has to be copied AND
# rewritten, transitively, because the dylibs reference each other the same way.
#
# Usage: bundle-macos.sh <package-dir> [<package-dir>...]
set -euo pipefail

# Paths that are part of the OS and exist on every Mac. Everything else is
# someone's package manager and has to come along. /usr/lib and /System are
# also inside the dyld shared cache, so they cannot be copied even if we tried.
is_system() {
	case "$1" in
		/usr/lib/*|/System/*|/Library/Frameworks/*) return 0 ;;
		*) return 1 ;;
	esac
}

# A dylib's own install id appears in `otool -L` exactly like a dependency
# does, so it has to be filtered out or the library looks like it depends on
# itself - and then "@rpath/libPyrosEngine.dylib" gets reported as an
# unresolved reference in a package that is actually fine.
own_id() {
	otool -D "$1" 2>/dev/null | tail -n +2
}

# otool prints "\t<path> (compatibility version ...)"; the first line names the
# file being inspected.
deps_of() {
	local id
	id="$(own_id "$1")"
	otool -L "$1" | tail -n +2 | awk '{print $1}' | while IFS= read -r d; do
		[ -n "$id" ] && [ "$d" = "$id" ] && continue
		echo "$d"
	done
}

bundle_dir() {
	local dir="$1"
	echo "--- bundling $dir ---"

	# Normalise what is already here before walking. CMake emits
	# libPyrosEngine.dylib with an id of @rpath/libPyrosEngine.dylib, which
	# only resolves if whoever loads it happens to carry a matching rpath -
	# true in the build tree, not true in a folder someone downloaded.
	local lib
	for lib in "$dir"/*.dylib; do
		[ -f "$lib" ] || continue
		chmod u+w "$lib"
		install_name_tool -id "@executable_path/$(basename "$lib")" "$lib" 2>/dev/null || true
	done

	walk "$dir"

	# Homebrew's "sdl2" is sdl2-compat: a shim that implements the SDL2 API on
	# top of SDL3 and dlopen()s libSDL3.dylib at runtime. A dlopen leaves no
	# load command, so the walk above cannot see it, and the package
	# looks complete while dying on any machine without Homebrew's sdl3. The
	# shim probes @loader_path/libSDL3.dylib first, so dropping it in the
	# folder is enough - but only because we know to put it there.
	if [ -f "$dir/libSDL2-2.0.0.dylib" ] && [ ! -f "$dir/libSDL3.dylib" ]; then
		local sdl3=""
		local p
		for p in "$(brew --prefix sdl3 2>/dev/null)/lib" "$(brew --prefix 2>/dev/null)/lib"; do
			if [ -f "$p/libSDL3.dylib" ]; then sdl3="$p/libSDL3.dylib"; break; fi
		done
		if [ -n "$sdl3" ]; then
			echo "bundling dlopen()ed $sdl3"
			cp -L "$sdl3" "$dir/libSDL3.dylib"
			chmod u+w "$dir/libSDL3.dylib"
			install_name_tool -id "@executable_path/libSDL3.dylib" "$dir/libSDL3.dylib"
		else
			echo "::warning::libSDL2-2.0.0.dylib is the sdl2-compat shim but libSDL3.dylib was not found - the package will not start"
		fi
	fi

	# SDL3 arrived after the first walk, so walk again to bundle ITS
	# dependencies. Everything already rewritten is a no-op the second time.
	walk "$dir"
}

# One pass over every Mach-O in the directory: copy in what it links from
# outside the package, and repoint it at the copy. Re-entrant by design.
walk() {
	local dir="$1"

	# Executables AND dylibs seed the walk. Dylibs matter because a Homebrew
	# .dylib is not mode u+x, so an executables-only seed would skip both the
	# SDL3 copied in above and libPyrosEngine.dylib itself, leaving their own
	# dependencies unbundled. Copies discovered mid-walk get appended.
	local queue=()
	while IFS= read -r f; do queue+=("$f"); done < <(
		find "$dir" -maxdepth 1 -type f \( -perm -u+x -o -name '*.dylib' \) -exec sh -c \
			'file -b "$1" | grep -q "Mach-O" && echo "$1"' _ {} \;
	)

	local -a seen=()
	while [ ${#queue[@]} -gt 0 ]; do
		local bin="${queue[0]}"
		queue=("${queue[@]:1}")

		case " ${seen[*]:-} " in *" $bin "*) continue ;; esac
		seen+=("$bin")

		local dep
		for dep in $(deps_of "$bin"); do
			is_system "$dep" && continue

			local base
			base="$(basename "$dep")"

			# @rpath/libPyrosEngine.dylib - our own library, which CMake
			# already emitted into the package. Just re-point it; there is
			# nothing to copy and no rpath to guess.
			if [ "${dep#@}" != "$dep" ]; then
				if [ -f "$dir/$base" ]; then
					install_name_tool -change "$dep" "@executable_path/$base" "$bin" 2>/dev/null || true
				fi
				continue
			fi

			# A real absolute path into Homebrew or a downloaded SDK.
			if [ ! -f "$dir/$base" ]; then
				if [ ! -f "$dep" ]; then
					echo "::warning::$dep (needed by $(basename "$bin")) does not exist - skipping"
					continue
				fi
				cp -L "$dep" "$dir/$base"
				chmod u+w "$dir/$base"
				# Its own id must stop being the Homebrew path too, or anything
				# linking it later records that absolute path all over again.
				install_name_tool -id "@executable_path/$base" "$dir/$base"
				queue+=("$dir/$base")
			fi

			install_name_tool -change "$dep" "@executable_path/$base" "$bin" 2>/dev/null || true
		done
	done

	# Editing a Mach-O invalidates its code signature, and on Apple Silicon an
	# invalid signature is fatal at load - the process is killed outright,
	# which reads like a corrupt download rather than a packaging step. An
	# ad-hoc signature is enough for a binary the user runs themselves.
	find "$dir" -maxdepth 1 -type f \( -perm -u+x -o -name '*.dylib' \) -exec sh -c \
		'file -b "$1" | grep -q "Mach-O" && codesign --force --sign - "$1" 2>/dev/null || true' _ {} \;
}

# Prove the result: nothing outside the package, and nothing left pointing at a
# build-machine path. Same reasoning as the Windows "Verify dependency closure"
# step - a package that links fine and dies on startup keeps CI green otherwise.
verify_dir() {
	local dir="$1"
	local failed=0
	local bin dep base

	# An empty walk must never read as success - see the equivalent guard in
	# bundle-linux.sh. Less likely to fire here, since `file` is part of macOS,
	# but a packaging step that copied nothing would otherwise pass silently.
	local count
	count=$(find "$dir" -maxdepth 1 -type f \( -perm -u+x -o -name '*.dylib' \) -exec sh -c \
		'file -b "$1" | grep -q "Mach-O" && echo "$1"' _ {} \; | wc -l)
	if [ "$count" -eq 0 ]; then
		echo "::error::found no Mach-O binaries in $dir"
		return 1
	fi
	while IFS= read -r bin; do
		for dep in $(deps_of "$bin"); do
			is_system "$dep" && continue
			case "$dep" in
				@executable_path/*|@loader_path/*)
					base="$(basename "$dep")"
					if [ ! -f "$dir/$base" ]; then
						echo "::error::$(basename "$bin") wants $dep, which is not in the package"
						failed=1
					fi
					;;
				*)
					echo "::error::$(basename "$bin") still links the build machine's $dep"
					failed=1
					;;
			esac
		done
	done < <(find "$dir" -maxdepth 1 -type f \( -perm -u+x -o -name '*.dylib' \) -exec sh -c \
		'file -b "$1" | grep -q "Mach-O" && echo "$1"' _ {} \;)
	return $failed
}

rc=0
for d in "$@"; do
	bundle_dir "$d"
done
for d in "$@"; do
	echo "--- verifying $d ---"
	verify_dir "$d" || rc=1
done
if [ $rc -ne 0 ]; then
	echo "packaging left unresolved dependencies"
	exit 1
fi
echo "all packages are self-contained"
