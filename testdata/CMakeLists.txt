# TESTDATA_DIR
set(TESTDATA_DIR ${CMAKE_SOURCE_DIR}/testdata)
# test_diff_patch
function(test_diff_patch name oldfile newfile patchfile newfile_test patchfile_test)
    add_test(NAME TestDiff_${name}
        COMMAND ../bsdiff ${TESTDATA_DIR}/${oldfile} ${TESTDATA_DIR}/${newfile} ${patchfile_test})
    add_test(NAME TestDiff_${name}_cmp
        COMMAND ${CMAKE_COMMAND} -E compare_files ${patchfile_test} ${TESTDATA_DIR}/${patchfile})
    set_tests_properties(TestDiff_${name}_cmp PROPERTIES DEPENDS TestDiff_${name})

    add_test(NAME TestPatch_${name}
        COMMAND ../bspatch ${TESTDATA_DIR}/${oldfile} ${newfile_test} ${TESTDATA_DIR}/${patchfile})
    add_test(NAME TestPatch_${name}_cmp
        COMMAND ${CMAKE_COMMAND} -E compare_files ${newfile_test} ${TESTDATA_DIR}/${newfile})
    set_tests_properties(TestPatch_${name}_cmp PROPERTIES DEPENDS TestPatch_${name})
endfunction()

test_diff_patch(simple
    "simple/v1"
    "simple/v2"
    "simple/v1_v2.patch"
    "v2.test"
    "v1_v2.patch.test")

test_diff_patch(putty1
    "putty/0.75.exe"
    "putty/0.76.exe"
    "putty/0.75_0.76.patch"
    "0.76.exe.test"
    "0.75_0.76.patch.test")
test_diff_patch(putty2
    "putty/0.76.exe"
    "putty/0.77.exe"
    "putty/0.76_0.77.patch"
    "0.77.exe.test"
    "0.76_0.77.patch.test")
test_diff_patch(putty3
    "putty/0.75.exe"
    "putty/0.77.exe"
    "putty/0.75_0.77.patch"
    "0.77.exe.test"
    "0.75_0.77.patch.test")

test_diff_patch(WinMerge1
    "WinMerge/2.16.14.exe"
    "WinMerge/2.16.16.exe"
    "WinMerge/2.16.14_2.16.16.patch"
    "2.16.16.exe.test"
    "2.16.14_2.16.16.patch.test")
test_diff_patch(WinMerge2
    "WinMerge/2.16.16.exe"
    "WinMerge/2.16.22.exe"
    "WinMerge/2.16.16_2.16.22.patch"
    "2.16.22.exe.test"
    "2.16.16_2.16.22.patch.test")
test_diff_patch(WinMerge3
    "WinMerge/2.16.14.exe"
    "WinMerge/2.16.22.exe"
    "WinMerge/2.16.14_2.16.22.patch"
    "2.16.22.exe.test"
    "2.16.14_2.16.22.patch.test")
