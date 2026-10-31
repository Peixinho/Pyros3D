# cmake -DOUTPUT=<file.inc> -DDIR=<dir> -DFILES=a,b,c -P EmbedShaderSources.cmake
# Each file as { "name", R"P3D(text)P3D" }, - the renderer compiles a material's surface against these at run time.
set(content "")
string(REPLACE "," ";" FILES "${FILES}")
foreach(f ${FILES})
	file(READ "${DIR}/${f}" text)
	string(APPEND content "{ \"${f}\", R\"P3D(${text})P3D\" },\n")
endforeach()
file(WRITE "${OUTPUT}.tmp" "${content}")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OUTPUT}.tmp" "${OUTPUT}")
