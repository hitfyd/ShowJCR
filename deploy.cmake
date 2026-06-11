# ShowJCR 部署脚本
# 用法: cmake -DEXE_PATH=<exe路径> -DQT_BIN_DIR=<Qt/bin路径> -P deploy.cmake
#
# 功能:
#   1. 运行 windeployqt，跳过不需要的插件类型和库
#   2. 清理 windeployqt 过度部署的文件（未使用的SQL驱动、图片格式、OpenGL软件渲染器等）
#   3. 输出部署结果和节省的体积

if(NOT EXE_PATH)
    message(FATAL_ERROR "EXE_PATH 未定义，用法: cmake -DEXE_PATH=<path> -DQT_BIN_DIR=<path> -P deploy.cmake")
endif()
if(NOT QT_BIN_DIR)
    message(FATAL_ERROR "QT_BIN_DIR 未定义，用法: cmake -DEXE_PATH=<path> -DQT_BIN_DIR=<path> -P deploy.cmake")
endif()

set(WINDEPLOYQT "${QT_BIN_DIR}/windeployqt.exe")

if(NOT EXISTS "${WINDEPLOYQT}")
    message(FATAL_ERROR "找不到 windeployqt.exe: ${WINDEPLOYQT}")
endif()

get_filename_component(EXE_DIR "${EXE_PATH}" DIRECTORY)

# ============================================================
# Step 1: 运行 windeployqt
# ============================================================
message(STATUS "=== Step 1/3: 运行 windeployqt ===")

# 注意：不能加 --release 参数！
# llvm-mingw Qt 预编译包的插件 PE 头被错误标记为 "debug"，
# 加 --release 会导致 windeployqt 过滤掉所有插件（包括必需的 qwindows.dll）。
#
# 被跳过的插件类型及原因:
#   generic            -> qtuiotouchplugin.dll (TUIO触摸输入) → 不必要，且会拖入 Qt6Network
#   networkinformation -> qnetworklistmanager.dll → 不必要
#   tls                -> SSL/TLS 后端 → 不必要（无网络请求）
# 被跳过的库:
#   --no-translations        -> 翻译文件（不需要）
#   --no-compiler-runtime    -> 编译器运行时（llvm-mingw 自带 libc++.dll）
#   --no-opengl-sw           -> opengl32sw.dll (~20MB 软件OpenGL渲染器)
#   --no-system-d3d-compiler -> D3Dcompiler_47.dll (~4MB D3D着色器编译器，Widgets应用不需要)
execute_process(
    COMMAND "${WINDEPLOYQT}" "${EXE_DIR}"
        --no-translations
        --no-compiler-runtime
        --no-system-d3d-compiler
        --no-opengl-sw
        --skip-plugin-types "generic,networkinformation,tls"
    RESULT_VARIABLE deploy_result
    OUTPUT_VARIABLE deploy_output
    ERROR_VARIABLE deploy_error
)
message(STATUS "${deploy_output}")
if(NOT deploy_result EQUAL 0)
    message(WARNING "windeployqt 返回非零退出码: ${deploy_result}")
    message(WARNING "${deploy_error}")
endif()

# ============================================================
# Step 2: 清理过度部署的文件
# ============================================================
message(STATUS "=== Step 2/3: 清理冗余文件 ===")

# 辅助函数：删除文件/目录并报告大小
function(remove_path path description)
    set(total_size 0)
    if(EXISTS "${path}")
        if(IS_DIRECTORY "${path}")
            file(GLOB_RECURSE files "${path}/*")
            foreach(f ${files})
                if(EXISTS "${f}")
                    file(SIZE "${f}" sz)
                    math(EXPR total_size "${total_size} + ${sz}")
                endif()
            endforeach()
            file(REMOVE_RECURSE "${path}")
        else()
            file(SIZE "${path}" sz)
            math(EXPR total_size "${total_size} + ${sz}")
            file(REMOVE "${path}")
        endif()
        math(EXPR total_kb "${total_size} / 1024")
        message(STATUS "  已删除: ${description} (${total_kb} KB)")
    else()
        message(STATUS "  已跳过: ${description} (不存在)")
    endif()
endfunction()

# ---- 未使用的 SQL 驱动（项目仅使用 SQLite） ----
# windeployqt 会部署 sqldrivers/ 下全部 4 个驱动，实际只需 qsqlite.dll
remove_path("${EXE_DIR}/sqldrivers/qsqlmimer.dll"  "qsqlmimer.dll (不使用Mimer数据库)")
remove_path("${EXE_DIR}/sqldrivers/qsqlodbc.dll"    "qsqlodbc.dll (不使用ODBC)")
remove_path("${EXE_DIR}/sqldrivers/qsqlpsql.dll"    "qsqlpsql.dll (不使用PostgreSQL)")

# ---- 未使用的图片格式插件（项目仅使用 JPEG + SVG） ----
remove_path("${EXE_DIR}/imageformats/qgif.dll"      "qgif.dll (不使用GIF格式)")
remove_path("${EXE_DIR}/imageformats/qico.dll"      "qico.dll (不使用ICO格式)")

# ---- 以下为安全网清理（windeployqt 参数已排除，但做二次确认） ----

# Qt6Network.dll + 相关插件（由 generic/qtuiotouchplugin.dll 拖入）
remove_path("${EXE_DIR}/Qt6Network.dll"             "Qt6Network.dll (不被项目使用)")
remove_path("${EXE_DIR}/generic"                    "generic/ (触摸输入插件)")
remove_path("${EXE_DIR}/networkinformation"         "networkinformation/ (网络状态插件)")
remove_path("${EXE_DIR}/tls"                        "tls/ (SSL/TLS 后端)")

# opengl32sw.dll (~20MB 软件OpenGL渲染器，Widgets应用不需要)
remove_path("${EXE_DIR}/opengl32sw.dll"             "opengl32sw.dll (软件OpenGL渲染器)")

# D3Dcompiler_47.dll (~4MB D3D着色器编译器，Widgets应用不需要)
remove_path("${EXE_DIR}/D3Dcompiler_47.dll"         "D3Dcompiler_47.dll (D3D着色器编译器)")

# 翻译文件目录
remove_path("${EXE_DIR}/translations"               "translations/ (翻译文件)")

# ============================================================
# Step 3: 输出部署摘要
# ============================================================
message(STATUS "=== Step 3/3: 部署摘要 ===")
message(STATUS "目标目录: ${EXE_DIR}")
message(STATUS "部署文件清单:")
file(GLOB deploy_files LIST_DIRECTORIES true "${EXE_DIR}/*")
foreach(f ${deploy_files})
    get_filename_component(fname "${f}" NAME)
    if(IS_DIRECTORY "${f}")
        file(GLOB_RECURSE plugin_contents "${f}/*")
        set(plugin_kb 0)
        foreach(pf ${plugin_contents})
            if(EXISTS "${pf}")
                file(SIZE "${pf}" sz)
                math(EXPR plugin_kb "${plugin_kb} + ${sz}")
            endif()
        endforeach()
        math(EXPR plugin_kb "${plugin_kb} / 1024")
        message(STATUS "  ${fname}/ (${plugin_kb} KB)")
    else()
        file(SIZE "${f}" sz)
        math(EXPR sz_kb "${sz} / 1024")
        message(STATUS "  ${fname} (${sz_kb} KB)")
    endif()
endforeach()

message(STATUS "")
message(STATUS "部署完成！分发目录: ${EXE_DIR}")
