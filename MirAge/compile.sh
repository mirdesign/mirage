HARDWARE_CONTROL_CUDA="CUDA"
HARDWARE_CONTROL_OPENCL="OPENCL"
HARDWARE_CONTROL_NOGPU="NOGPU"
HARDWARE_CONTROL_ASKUSER="ASKUSER"
HARDWARE_CONTROL="$HARDWARE_CONTROL_ASKUSER"

COMPILE_SETTING_PERFORMANCE="-O3"

FILES_OPENCL="Analyser.cpp Screen.cpp Enum.cpp SignalHandler.cpp Hardware.cpp MultiThreading.cpp Sequences.cpp Hashmap.cpp LogToScreen.cpp Files.cpp Display.cpp MatrixCPU.cpp LogManager.cpp Configuration.cpp MatrixGPU.cpp MatrixResult.cpp  GPUDevice.cpp MultiNodePlatform.cpp MirAge.cpp"
FILES_CUDA="$FILES_OPENCL MatrixGPUCU.cu"


# Ask user what compilation method we need to use
if [ "$HARDWARE_CONTROL" = "$HARDWARE_CONTROL_ASKUSER" ] 
then
    bold=$(tput bold)
    normal=$(tput sgr0)

    echo ""
    echo "You can choose to build MirAge with CUDA, OpenCL or no GPU support"
    echo "Make sure you installed the right libraries for CUDA or OpenCL."
    echo ""
    echo "How to install ${bold}CUDA${normal}?              Please visit https://developer.nvidia.com/cuda-downloads"
    echo "How to install ${bold}OpenCL for Nvidia${normal}? Please visit https://developer.nvidia.com/cuda-downloads"
    echo "How to install ${bold}OpenCL for AMD${normal}?    Please visit https://support.amd.com/en-us/kb-articles/Pages/OpenCL2-Driver.aspx"
    echo ""
    echo "Please enter the number you need and press RETURN."
    echo ""
    echo "     1) ${bold}CUDA${normal}"
    echo "     2) ${bold}OpenCL${normal} [Recommended]"
    echo "     3) ${bold}No GPU${normal} [Default]"
    echo ""
    echo "     4) Quit"
    echo ""
    echo "Enter your choice:"
    read desiredCompileMethod

    if [ "$desiredCompileMethod" = "1" ] 
    then
        HARDWARE_CONTROL="$HARDWARE_CONTROL_CUDA"
    elif [ "$desiredCompileMethod" = "2" ] 
    then
        HARDWARE_CONTROL="$HARDWARE_CONTROL_OPENCL"
    elif [ "$desiredCompileMethod" = "3" ] 
    then
        HARDWARE_CONTROL="$HARDWARE_CONTROL_NOGPU"
    elif [ "$desiredCompileMethod" = "4" ] 
    then
        exit
    else
        HARDWARE_CONTROL="$HARDWARE_CONTROL_NOGPU"
        #echo "Don't know what to do with your answer '$desiredCompileMethod'."
        #exit
    fi
fi

    echo ""
 

# Delete old application
echo Deleting old mirage.out
rm mirage.out 2> /dev/null


# Load modules


# Compile!
echo "Compiling MirAge with '$HARDWARE_CONTROL' support. Please wait.."
if [ "$HARDWARE_CONTROL" = "$HARDWARE_CONTROL_CUDA" ] 
then
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/hpc/sw/cuda/7.5.18/lib64
    module load cuda/7.5.18
    module load clblas
    nvcc $COMPILE_SETTING_PERFORMANCE $FILES_CUDA -D__CUDA__ -std=c++11 -o mirage.out -I$CUDA_HOME/include -I/hpc/sw/clblas-2.2.0/include -L/hpc/sw/cuda/7.5.18/lib64 -lclBLAS -lcudart -lineinfo --use_fast_math --maxrregcount=32 --ptxas-options=-v -G -g -deviceemu
elif [ "$HARDWARE_CONTROL" = "$HARDWARE_CONTROL_OPENCL" ] 
then
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/hpc/sw/cuda/7.5.18/lib64
    module load cuda/7.5.18
    g++ $COMPILE_SETTING_PERFORMANCE $FILES_OPENCL -D__OPENCL__ -std=c++11 -o mirage.out -I$CUDA_HOME/include -lOpenCL -lm -lpthread
elif [ "$HARDWARE_CONTROL" = "$HARDWARE_CONTROL_NOGPU" ] 
then
    g++ $COMPILE_SETTING_PERFORMANCE $FILES_OPENCL -D__linux__ -std=c++14 -o mirage.out -lm -lpthread -fpermissive
else
    echo "Oops! Don't know what to do with $HARDWARE_CONTROL"
fi
echo Ready! Enjoy :\)






