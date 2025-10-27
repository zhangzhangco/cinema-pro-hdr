#!/bin/bash

# Cinema Pro HDR DCTL 构建和测试脚本
# 
# 用途：构建DCTL相关组件并运行验证测试
# 支持：Linux, macOS, Windows (WSL)

set -e  # 遇到错误时退出

# 脚本配置
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
DCTL_DIR="$PROJECT_ROOT/src/dctl"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检查依赖
check_dependencies() {
    log_info "检查构建依赖..."
    
    # 检查CMake
    if ! command -v cmake &> /dev/null; then
        log_error "CMake未安装，请先安装CMake"
        exit 1
    fi
    
    # 检查编译器
    if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
        log_error "未找到C++编译器，请安装g++或clang++"
        exit 1
    fi
    
    log_success "依赖检查通过"
}

# 创建构建目录
setup_build_dir() {
    log_info "设置构建目录..."
    
    if [ -d "$BUILD_DIR" ]; then
        log_warning "构建目录已存在，清理中..."
        rm -rf "$BUILD_DIR"
    fi
    
    mkdir -p "$BUILD_DIR"
    log_success "构建目录已创建: $BUILD_DIR"
}

# 配置CMake
configure_cmake() {
    log_info "配置CMake..."
    
    cd "$BUILD_DIR"
    
    # CMake配置选项
    CMAKE_OPTIONS=(
        "-DCMAKE_BUILD_TYPE=Release"
        "-DBUILD_TESTS=ON"
        "-DBUILD_TOOLS=ON"
        "-DBUILD_OFX=OFF"  # DCTL测试不需要OFX
    )
    
    # 平台特定配置
    if [[ "$OSTYPE" == "darwin"* ]]; then
        CMAKE_OPTIONS+=("-DUSE_METAL=ON")
        log_info "检测到macOS，启用Metal支持"
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        # 检查CUDA
        if command -v nvcc &> /dev/null; then
            CMAKE_OPTIONS+=("-DUSE_CUDA=ON")
            log_info "检测到CUDA，启用CUDA支持"
        fi
    fi
    
    # 运行CMake配置
    if cmake "${CMAKE_OPTIONS[@]}" "$PROJECT_ROOT"; then
        log_success "CMake配置成功"
    else
        log_error "CMake配置失败"
        exit 1
    fi
}

# 构建项目
build_project() {
    log_info "构建项目..."
    
    cd "$BUILD_DIR"
    
    # 检测CPU核心数
    if [[ "$OSTYPE" == "darwin"* ]]; then
        CORES=$(sysctl -n hw.ncpu)
    else
        CORES=$(nproc)
    fi
    
    log_info "使用 $CORES 个并行任务构建"
    
    if make -j"$CORES"; then
        log_success "项目构建成功"
    else
        log_error "项目构建失败"
        exit 1
    fi
}

# 运行DCTL验证测试
run_dctl_tests() {
    log_info "运行DCTL验证测试..."
    
    cd "$BUILD_DIR"
    
    # 检查测试可执行文件是否存在
    if [ ! -f "./test_dctl_validation" ]; then
        log_error "DCTL验证测试可执行文件未找到"
        exit 1
    fi
    
    log_info "执行DCTL验证测试..."
    if ./test_dctl_validation; then
        log_success "DCTL验证测试通过"
    else
        log_error "DCTL验证测试失败"
        exit 1
    fi
}

# 运行单元测试
run_unit_tests() {
    log_info "运行单元测试..."
    
    cd "$BUILD_DIR"
    
    if make test; then
        log_success "单元测试通过"
    else
        log_warning "部分单元测试失败，请检查详细输出"
    fi
}

# 验证DCTL文件
validate_dctl_files() {
    log_info "验证DCTL文件..."
    
    # 检查DCTL文件是否存在
    DCTL_FILES=(
        "$DCTL_DIR/cinema_pro_hdr.dctl"
        "$DCTL_DIR/cinema_pro_hdr_optimized.dctl"
    )
    
    for dctl_file in "${DCTL_FILES[@]}"; do
        if [ -f "$dctl_file" ]; then
            log_success "DCTL文件存在: $(basename "$dctl_file")"
            
            # 基本语法检查（检查是否包含必要的函数）
            if grep -q "__DEVICE__ float3 transform" "$dctl_file"; then
                log_success "DCTL语法检查通过: $(basename "$dctl_file")"
            else
                log_warning "DCTL语法可能有问题: $(basename "$dctl_file")"
            fi
        else
            log_error "DCTL文件缺失: $(basename "$dctl_file")"
        fi
    done
}

# 生成性能报告
generate_performance_report() {
    log_info "生成性能报告..."
    
    cd "$BUILD_DIR"
    
    # 运行性能基准测试
    if [ -f "./test_dctl_validation" ]; then
        log_info "运行性能基准测试..."
        
        # 创建报告目录
        REPORT_DIR="$BUILD_DIR/dctl_reports"
        mkdir -p "$REPORT_DIR"
        
        # 生成性能报告
        ./test_dctl_validation > "$REPORT_DIR/dctl_validation_report.txt" 2>&1 || true
        
        if [ -f "$REPORT_DIR/dctl_validation_report.txt" ]; then
            log_success "性能报告已生成: $REPORT_DIR/dctl_validation_report.txt"
            
            # 显示报告摘要
            log_info "性能报告摘要:"
            grep -E "(性能测试结果|每样本时间|吞吐量)" "$REPORT_DIR/dctl_validation_report.txt" || true
        fi
    fi
}

# 安装DCTL文件到Resolve目录
install_dctl_files() {
    log_info "安装DCTL文件到DaVinci Resolve..."
    
    # 检测Resolve LUT目录
    RESOLVE_LUT_DIRS=()
    
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS
        RESOLVE_LUT_DIRS+=(
            "$HOME/Library/Application Support/Blackmagic Design/DaVinci Resolve/LUT"
            "/Library/Application Support/Blackmagic Design/DaVinci Resolve/LUT"
        )
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        # Linux
        RESOLVE_LUT_DIRS+=(
            "$HOME/.local/share/DaVinciResolve/LUT"
            "/opt/resolve/LUT"
        )
    elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
        # Windows (WSL/Cygwin)
        RESOLVE_LUT_DIRS+=(
            "$APPDATA/Blackmagic Design/DaVinci Resolve/Support/LUT"
            "/c/ProgramData/Blackmagic Design/DaVinci Resolve/Support/LUT"
        )
    fi
    
    # 查找可用的Resolve目录
    INSTALL_DIR=""
    for dir in "${RESOLVE_LUT_DIRS[@]}"; do
        if [ -d "$dir" ]; then
            INSTALL_DIR="$dir"
            break
        fi
    done
    
    if [ -n "$INSTALL_DIR" ]; then
        log_info "找到DaVinci Resolve LUT目录: $INSTALL_DIR"
        
        # 复制DCTL文件
        for dctl_file in "$DCTL_DIR"/*.dctl; do
            if [ -f "$dctl_file" ]; then
                cp "$dctl_file" "$INSTALL_DIR/"
                log_success "已安装: $(basename "$dctl_file")"
            fi
        done
        
        log_success "DCTL文件安装完成"
        log_info "请重启DaVinci Resolve以加载新的DCTL文件"
    else
        log_warning "未找到DaVinci Resolve LUT目录"
        log_info "请手动将以下文件复制到Resolve的LUT目录:"
        for dctl_file in "$DCTL_DIR"/*.dctl; do
            if [ -f "$dctl_file" ]; then
                log_info "  - $(basename "$dctl_file")"
            fi
        done
    fi
}

# 清理构建文件
cleanup() {
    if [ "$1" = "--clean" ]; then
        log_info "清理构建文件..."
        rm -rf "$BUILD_DIR"
        log_success "构建文件已清理"
    fi
}

# 显示帮助信息
show_help() {
    echo "Cinema Pro HDR DCTL 构建脚本"
    echo ""
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  --help          显示此帮助信息"
    echo "  --clean         清理构建文件"
    echo "  --install       安装DCTL文件到Resolve"
    echo "  --test-only     仅运行测试，不重新构建"
    echo "  --no-install    构建但不安装DCTL文件"
    echo ""
    echo "示例:"
    echo "  $0              # 完整构建和测试"
    echo "  $0 --clean      # 清理构建文件"
    echo "  $0 --install    # 仅安装DCTL文件"
    echo "  $0 --test-only  # 仅运行测试"
}

# 主函数
main() {
    log_info "Cinema Pro HDR DCTL 构建脚本启动"
    log_info "项目根目录: $PROJECT_ROOT"
    
    # 解析命令行参数
    INSTALL_DCTL=true
    TEST_ONLY=false
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --help)
                show_help
                exit 0
                ;;
            --clean)
                cleanup --clean
                exit 0
                ;;
            --install)
                install_dctl_files
                exit 0
                ;;
            --test-only)
                TEST_ONLY=true
                shift
                ;;
            --no-install)
                INSTALL_DCTL=false
                shift
                ;;
            *)
                log_error "未知选项: $1"
                show_help
                exit 1
                ;;
        esac
    done
    
    # 执行构建流程
    if [ "$TEST_ONLY" = false ]; then
        check_dependencies
        setup_build_dir
        configure_cmake
        build_project
    fi
    
    # 验证DCTL文件
    validate_dctl_files
    
    # 运行测试
    run_dctl_tests
    run_unit_tests
    
    # 生成报告
    generate_performance_report
    
    # 安装DCTL文件
    if [ "$INSTALL_DCTL" = true ]; then
        install_dctl_files
    fi
    
    log_success "DCTL构建和测试完成！"
    
    # 显示后续步骤
    echo ""
    log_info "后续步骤:"
    echo "1. 在DaVinci Resolve中测试DCTL文件"
    echo "2. 检查性能报告: $BUILD_DIR/dctl_reports/"
    echo "3. 根据需要调整参数和优化"
}

# 脚本入口点
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi