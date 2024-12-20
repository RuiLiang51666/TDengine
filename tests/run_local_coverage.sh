#!/bin/bash

# Color setting
RED='\033[0;31m'
GREEN='\033[1;32m'
GREEN_DARK='\033[0;32m'
GREEN_UNDERLINE='\033[4;32m'
NC='\033[0m'

function print_color() {
    local color="$1"
    local message="$2"
    echo -e "${color}${message}${NC}"
}

function printHelp() {
    echo "Usage: $(basename $0) [options]"
    echo
    echo "Options:"
    echo "  -d [Project dir]          Project directory (default: outermost project directory)"
    echo "                            e.g., -d /home/TDinternal/community"
    echo "  -i [Build test branch]    Build test branch (default: no)"
    echo "                            Options: yes (build and install), no (install only)"
    echo "  -f [Capture gcda dir]     Capture gcda directory (default: <project dir>/debug)"
    echo "  -c [Test case]            Test single case or all cases (default: null)"
    echo "                            Options:"
    echo "                              -c all  : run all python, sim cases and unit cases"
    echo "                              -c task : run all python and sim cases"
    echo "                              -c cmd  : run the specified test command"
    echo "                            e.g., -c './test.sh -f tsim/stream/streamFwcIntervalFill.sim'"
    echo "  -u [Unit test case]       Unit test case (default: null)"
    echo "                            e.g., -u './schedulerTest'"
    exit 0
}

# Initialization parameter
PROJECT_DIR=""
CAPTURE_GCDA_DIR=""
TEST_CASE=""
UNIT_TEST_CASE=""
BRANCH_BUILD="NO"

# Parse command line parameters
while getopts "hd:f:c:u:i:" arg; do
    case $arg in
        d)
            PROJECT_DIR=$OPTARG
            ;;
        f)
            CAPTURE_GCDA_DIR=$OPTARG
            ;;
        c)
            TEST_CASE=$OPTARG
            ;;
        u)
            UNIT_TEST_CASE=$OPTARG
            ;;
        i)
            BRANCH_BUILD=$OPTARG
            ;;
        h)
            printHelp
            ;;
        ?)
            echo "Usage: ./$(basename $0) -h"
            exit 1
            ;;
    esac
done

# Find the current project directory if not specified
if [ -z "$PROJECT_DIR" ]; then
    CODE_DIR=$(dirname $0)
    cd $CODE_DIR
    CODE_DIR=$(pwd)
    if [[ "$CODE_DIR" == *"/community/"*  ]]; then
        PROJECT_DIR=$(realpath ../..)
    else
        PROJECT_DIR=$(realpath ..)
    fi
fi
if [[ "$PROJECT_DIR" == *"/TDinternal" ]]; then
    TDENGINE_DIR="$PROJECT_DIR/community"
else
    TDENGINE_DIR="$PROJECT_DIR"
fi
BUILD_DIR="$PROJECT_DIR/debug"

if [ -z "$CAPTURE_GCDA_DIR" ]; then
    CAPTURE_GCDA_DIR="$BUILD_DIR"
elif [[ "$(realpath $CAPTURE_GCDA_DIR)" != "$(realpath $BUILD_DIR)/"* ]]; then
    print_color $RED "CAPTURE_GCDA_DIR is not a subdirectory of BUILD_DIR"
    exit 1
fi

# Show all parameters
echo "PROJECT_DIR = $PROJECT_DIR"
echo "TDENGINE_DIR = $TDENGINE_DIR"
echo "BUILD_DIR = $BUILD_DIR"
echo "CAPTURE_GCDA_DIR = $CAPTURE_GCDA_DIR"
echo "TEST_CASE = $TEST_CASE"
echo "UNIT_TEST_CASE = $UNIT_TEST_CASE"
echo "BRANCH_BUILD = $BRANCH_BUILD"

today=`date +"%Y%m%d"`
TDENGINE_ALLCI_REPORT="$TDENGINE_DIR/tests/all-ci-report-$today.log"

function buildTDengine() {
    print_color "$GREEN" "TDengine build start"

    [ -d $BUILD_DIR ] || mkdir $BUILD_DIR
    cd $BUILD_DIR

    print_color "$GREEN" "rebuild.."
    rm -rf *
    makecmd="cmake .. -DCOVER=true -DJEMALLOC_ENABLED=false -DBUILD_HTTP=false -DWEBSOCKET=true -DBUILD_TOOLS=true -DBUILD_TEST=true -DBUILD_CONTRIB=true -DBUILD_ADDR2LINE=true -DBUILD_SANITIZER=false -DCMAKE_BUILD_TYPE=Debug"
    print_color "$GREEN" "$makecmd"
    $makecmd
    make -j $(nproc) install
}

if [ "$BRANCH_BUILD" = "YES" -o "$BRANCH_BUILD" = "yes" ]; then
    buildTDengine
else
    print_color "$GREEN" "Build is not required for this test!"
fi

function runCasesOneByOne () {
    while read -r line; do
        if [[ "$line" != "#"* ]]; then
            cmd=`echo $line | cut -d',' -f 5`
            if [[ "$2" == "sim" ]] && [[ $line == *"script"* ]]; then
                echo $cmd
                case=`echo $cmd | cut -d' ' -f 3`
                case_file=`echo $case | tr -d ' /' `
                start_time=`date +%s`
                date +%F\ %T | tee -a  $TDENGINE_ALLCI_REPORT  && timeout 20m $cmd > $TDENGINE_DIR/tests/$case_file.log 2>&1 && \
                echo -e "${GREEN}$case success${NC}" | tee -a  $TDENGINE_ALLCI_REPORT || \
                echo -e "${RED}$case failed${NC}" | tee -a $TDENGINE_ALLCI_REPORT
                end_time=`date +%s`
                echo execution time of $case was `expr $end_time - $start_time`s. | tee -a $TDENGINE_ALLCI_REPORT

            elif [[ "$line" == *"$2"* ]]; then
                echo $cmd
                if [[ "$cmd" == *"pytest.sh"* ]]; then
                    cmd=`echo $cmd | cut -d' ' -f 2-20`
                fi
                case=`echo $cmd | cut -d' ' -f 4-20`
                case_file=`echo $case | tr -d ' /' `
                start_time=`date +%s`
                date +%F\ %T | tee -a $TDENGINE_ALLCI_REPORT && timeout 20m $cmd > $TDENGINE_DIR/tests/$case_file.log 2>&1 && \
                echo -e "${GREEN}$case success${NC}" | tee -a $TDENGINE_ALLCI_REPORT || \
                echo -e "${RED}$case failed${NC}" | tee -a $TDENGINE_ALLCI_REPORT
                end_time=`date +%s`
                echo execution time of $case was `expr $end_time - $start_time`s. | tee -a $TDENGINE_ALLCI_REPORT
            fi
        fi
    done < $1
}

function runUnitTest() {
    print_color "$GREEN" "=== Run unit test case ==="
    print_color "$GREEN" "cd $BUILD_DIR"
    cd $BUILD_DIR
    ctest -j $(nproc)
    print_color "$GREEN" "3.0 unit test done"
}

function runSimCases() {
    print_color "$GREEN" "=== Run sim cases ==="

    cd $TDENGINE_DIR/tests/script
    runCasesOneByOne $TDENGINE_DIR/tests/parallel_test/longtimeruning_cases.task sim

    totalSuccess=`grep 'sim success' $TDENGINE_ALLCI_REPORT | wc -l`
    if [ "$totalSuccess" -gt "0" ]; then
        print_color "$GREEN" "### Total $totalSuccess SIM test case(s) succeed! ###" | tee -a $TDENGINE_ALLCI_REPORT
    fi

    totalFailed=`grep 'sim failed\|fault' $TDENGINE_ALLCI_REPORT | wc -l`
    if [ "$totalFailed" -ne "0" ]; then
        print_color "$RED" "### Total $totalFailed SIM test case(s) failed! ###" | tee -a $TDENGINE_ALLCI_REPORT
    fi
}

function runPythonCases() {
    print_color "$GREEN" "=== Run python cases ==="

    cd $TDENGINE_DIR/tests/parallel_test
    sed -i '/compatibility.py/d' longtimeruning_cases.task

    # army
    cd $TDENGINE_DIR/tests/army
    runCasesOneByOne ../parallel_test/longtimeruning_cases.task army

    # system-test
    cd $TDENGINE_DIR/tests/system-test
    runCasesOneByOne ../parallel_test/longtimeruning_cases.task system-test

    # develop-test
    cd $TDENGINE_DIR/tests/develop-test
    runCasesOneByOne ../parallel_test/longtimeruning_cases.task develop-test

    totalSuccess=`grep 'py success' $TDENGINE_ALLCI_REPORT | wc -l`
    if [ "$totalSuccess" -gt "0" ]; then
        print_color "$GREEN" "### Total $totalSuccess python test case(s) succeed! ###" | tee -a $TDENGINE_ALLCI_REPORT
    fi

    totalFailed=`grep 'py failed\|fault' $TDENGINE_ALLCI_REPORT | wc -l`
    if [ "$totalFailed" -ne "0" ]; then
        print_color "$RED" "### Total $totalFailed python test case(s) failed! ###" | tee -a $TDENGINE_ALLCI_REPORT
    fi
}


function runTest_all() {
    print_color "$GREEN" "run Test"

    cd $TDENGINE_DIR
    [ -d sim ] && rm -rf sim
    [ -f $TDENGINE_ALLCI_REPORT ] && rm $TDENGINE_ALLCI_REPORT

    runUnitTest
    runSimCases
    runPythonCases

    stopTaosd
}


function runTest() {
    print_color "$GREEN" "run Test"

    cd $TDENGINE_DIR
    [ -d sim ] && rm -rf sim
    [ -f $TDENGINE_ALLCI_REPORT ] && rm $TDENGINE_ALLCI_REPORT

    if [ -n "$TEST_CASE" ] && [ "$TEST_CASE" != "all" ] && [ "$TEST_CASE" != "task" ]; then
        print_color "$GREEN" "Test case: $TEST_CASE "
        cd $TDENGINE_DIR/tests/script/ && $TEST_CASE
        cd $TDENGINE_DIR/tests/army/ && $TEST_CASE
        cd $TDENGINE_DIR/tests/system-test/ && $TEST_CASE
        cd $TDENGINE_DIR/tests/develop-test/ && $TEST_CASE
    elif [ "$TEST_CASE" == "all" ]; then
        print_color "$GREEN" "Test case is : parallel_test/longtimeruning_cases.task and all unit cases"
        runTest_all
    elif [ "$TEST_CASE" == "task" ]; then
        print_color "$GREEN" "Test case is only: parallel_test/longtimeruning_cases.task "
        runSimCases
        runPythonCases
    elif [ -n "$UNIT_TEST_CASE" ]; then
        print_color $GREEN "Unit test case: $UNIT_TEST_CASE"
        cd $BUILD_DIR/build/bin/ && $UNIT_TEST_CASE
    else
        print_color "$GREEN" "Test case is null"
    fi


    stopTaosd
}

function lcovFunc {
    echo "collect data by lcov"
    cd $BIULD_DIR

    print_color "$GREEN" "Test gcda file dir: $CAPTURE_GCDA_DIR "

    # collect data
    lcov -d "$CAPTURE_GCDA_DIR" -capture --rc lcov_branch_coverage=1 --rc genhtml_branch_coverage=1 --no-external --exclude "*/test/*" -b $PROJECT_DIR -o coverage.info
}

function stopTaosd {
    print_color "$GREEN" "Stop taosd start"
    systemctl stop taosd
    PID=`ps -ef|grep -w taosd | grep -v grep | awk '{print $2}'`
    while [ -n "$PID" ]
    do
        pkill -TERM -x taosd
        sleep 1
        PID=`ps -ef|grep -w taosd | grep -v grep | awk '{print $2}'`
    done
    print_color "$GREEN" "Stop tasod end"
}

function stopTaosadapter {
    print_color "$GREEN" "Stop taosadapter"
    systemctl stop taosadapter.service
    PID=`ps -ef|grep -w taosadapter | grep -v grep | awk '{print $2}'`
    while [ -n "$PID" ]
    do
        pkill -TERM -x taosadapter
        sleep 1
        PID=`ps -ef|grep -w taosd | grep -v grep | awk '{print $2}'`
    done
    print_color "$GREEN" "Stop tasoadapter end"

}

date >> $BUILD_DIR/date.log
print_color "$GREEN" "Run local coverage test cases" | tee -a $BUILD_DIR/date.log

stopTaosd

runTest

lcovFunc


date >> $BUILD_DIR/date.log
print_color "$GREEN" "End of local coverage test cases" | tee -a $BUILD_DIR/date.log


# Define coverage information files and output directories
COVERAGE_INFO="$BUILD_DIR/coverage.info"
OUTPUT_DIR="$BUILD_DIR/coverage_report"

# Check whether the coverage information file exists
if [ ! -f "$COVERAGE_INFO" ]; then
    echo "Error: $COVERAGE_INFO not found!"
    exit 1
fi

# Generate local HTML reports
genhtml "$COVERAGE_INFO"  --branch-coverage --function-coverage --output-directory "$OUTPUT_DIR"

# Check whether the report was generated successfully
if [ $? -eq 0 ]; then
    echo "HTML coverage report generated successfully in $OUTPUT_DIR"
    echo "For more details : "
    echo "http://192.168.1.61:7000/"
else
    echo "Error generating HTML coverage report"
    exit 1
fi

