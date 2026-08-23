# pyros_stage_dir(<source-dir> <dest-path>)
#
# Makes <source-dir> reachable as <dest-path> next to a freshly built binary.
# The engine loads "shaders/..." and the editor loads "assets/..." relative to
# the working directory, so both have to exist in the build tree.
#
# On macOS/Linux this is a symlink, deliberately: edits to a shader or an
# editor asset are then picked up on the next run with no rebuild. Windows
# reserves symlink creation for Developer Mode or an elevated process, so
# there it degrades to a post-configure copy - correct, just not live.

function(pyros_stage_dir source_dir dest_path)
	if (EXISTS ${dest_path})
		return()
	endif()

	if (WIN32)
		get_filename_component(_dest_parent ${dest_path} DIRECTORY)
		get_filename_component(_dest_name ${dest_path} NAME)
		get_filename_component(_source_name ${source_dir} NAME)
		file(COPY ${source_dir} DESTINATION ${_dest_parent})
		# file(COPY) keeps the source's own directory name; rename when the
		# caller asked for something else (resources/shaders -> shaders is a
		# no-op, editor/assets -> assets likewise, but don't assume).
		if (NOT _source_name STREQUAL _dest_name)
			file(RENAME ${_dest_parent}/${_source_name} ${dest_path})
		endif()
	else()
		file(CREATE_LINK ${source_dir} ${dest_path} SYMBOLIC)
	endif()
endfunction()
