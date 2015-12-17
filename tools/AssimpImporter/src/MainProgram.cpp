//============================================================================
// Name        : MainProgram.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Main
//============================================================================

#include "AssimpModelImporter.h"
#include "AssimpAnimationImporter.h"
#include <sstream>
#include <string>

using namespace std;
using namespace p3d;
using namespace p3d::Math;
/*
 * 
 */

void showHelp()
{
    std::cout << "Pyros3D Model and Animation Converter v0.1" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "--help -> Shows help" << std::endl;
    std::cout << "--model [model path] [model converted path] -> Converts Model to Pyros3D Format" << std::endl;
    std::cout << "--animation [animation path] [animation converted path] -> Converts Animation to Pyros3D Format" << std::endl;
    std::cout << "--animation-interval [animation path] [animation converted path] [custom animation name] [init] [end] -> Converts Animation to Pyros3D Format" << std::endl;
    std::cout << "no args -> Opens application Gui" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Press Enter to Continue..." << std::endl;
    cin.get(); 
    cin.get();
}

int main(int argc, char** argv) {

    std::string strHelp ("--help");
    std::string strAnimation ("--animation");
    std::string strModel ("--model");
    std::string strAnimationInterval ("--animation-interval");

    std::vector<std::string> args;
    for (int i=0;i<argc;i++)
    {
        args.push_back(argv[i]);
    }

    if (argc>=3)
    {
        if (strModel.compare(args[1])==0)
        {
            AssimpModelImporter* model = new AssimpModelImporter();
            if (model->Load(args[2]))
            {
                std::string modelOutput((argc==4?args[3]:args[2]) + ".p3dm");
                if (model->ConvertToPyrosFormat(modelOutput))
                    std::cout << "Model Converted to: " << modelOutput << std::endl;
                else std::cout << "Failed to Convert the Model: " << modelOutput << std::endl;
            }
            delete model;
        }
        else if (strAnimation.compare(args[1])==0)
        {
            AssimpAnimationImporter* model = new AssimpAnimationImporter();
            if (model->Load(args[2]))
            {
                std::string modelOutput((argc==4?args[3]:args[2]) + ".p3da");
                if (model->ConvertToPyrosFormat(modelOutput))
                    std::cout << "Animation Converted to: " << modelOutput << std::endl;
                else std::cout << "Failed to Convert the Animation: " << modelOutput << std::endl;
            }
            delete model;
        }
        else if (strAnimationInterval.compare(args[1])==0)
        {
            AssimpAnimationImporter* model = new AssimpAnimationImporter();
            int init1, end1;
            std::stringstream(args[5]) >> init1;
            std::stringstream(args[6]) >> end1;
            
            if (model->Load(args[2],init1,end1,args[4]))
            {
                std::string modelOutput(args[3] + ".p3da");
                if (model->ConvertToPyrosFormat(modelOutput))
                    std::cout << "Animation Converted to: " << modelOutput << std::endl;
                else std::cout << "Failed to Convert the Animation: " << modelOutput << std::endl;
            }
            delete model;
        }
    }

    else if (argc==2 && strHelp.compare(args[1])==0)
    {
        showHelp();
        
    } else {
        bool init = true;
        char o;
        while (init)
        {
            std::cout << "Pyros3D Model and Animation Converter v0.1" << std::endl;
            std::cout << "###################################" << std::endl;
            std::cout << "# 1 - Convert Model               #" << std::endl;
            std::cout << "# 2 - Convert Animation           #" << std::endl;
            std::cout << "# 3 - Convert Animation Interval  #" << std::endl;
            std::cout << "# 4 - Help                        #" << std::endl;
            std::cout << "#                                 #" << std::endl;
            std::cout << "# q - Quit                        #" << std::endl;
            std::cout << "###################################" << std::endl;
            std::cout << "By Peixinho" << std::endl;
            std::cout << ":";
            std::cin >> o;
            switch(o)
            {
                case '1':
                {
                    // Convert Model
                    std::cout << "Pyros3D Model and Animation Converter v0.1" << std::endl;
                    std::cout << "Type Model File Path" << std::endl;
                    std::cout << ":";
                    std::string modelPath;
                    std::cin >> modelPath;
                    AssimpModelImporter* model = new AssimpModelImporter();
                    if (model->Load(modelPath))
                    {
                        std::cout << "Type Output Name" << std::endl;
                        std::cout << ":";
                        std::string modelOutput;
                        std::cin >> modelOutput;
                        if (model->ConvertToPyrosFormat(modelOutput + ".p3dm"))
                            std::cout << "Converted to "<< modelOutput << ".p3dm" << " - Press Enter to Continue..." << std::endl;
                        else std::cout << "Failed - Press Enter to Continue..." << std::endl;
                        cin.get(); 
                        cin.get();
                        delete model;
                    } else {
                        std::cout << "Failed to Import Model, Press Enter to Continue..." << std::endl;
                        cin.get(); 
                        cin.get();
                        delete model;
                        break;
                    }
                }
                break;
                case '2':
                {
                    // Convert Animation
                    std::cout << "Pyros3D Model and Animation Converter v0.1" << std::endl;
                    std::cout << "Type Animation File Path" << std::endl;
                    std::cout << ":";
                    std::string modelPath;
                    std::cin >> modelPath;
                    AssimpAnimationImporter* model = new AssimpAnimationImporter();
                    if (model->Load(modelPath))
                    {
                        std::cout << "Type Output Name" << std::endl;
                        std::cout << ":";
                        std::string modelOutput;
                        std::cin >> modelOutput;
                        if (model->ConvertToPyrosFormat(modelOutput + ".p3da"))
                            std::cout << "Converted to "<< modelOutput << ".p3da" << " - Press Enter to Continue..." << std::endl;
                        else std::cout << "Failed - Press Enter to Continue..." << std::endl;
                        cin.get(); 
                        cin.get();
                        delete model;
                    } else {
                        std::cout << "Failed to Import Model, Enter to Continue..." << std::endl;
                        cin.get(); 
                        cin.get();
                        delete model;
                        break;
                    }
                }
                break;
                case '3':
                {
                    // Convert Animation
                    std::cout << "Pyros3D Model and Animation Converter v0.1" << std::endl;
                    std::cout << "Type Animation File Path" << std::endl;
                    std::cout << ":";
                    std::string modelPath;
                    std::cin >> modelPath;

                    std::cout << "Type Output Name" << std::endl;
                    std::cout << ":";
                    std::string modelOutput;
                    std::cin >> modelOutput;

                    std::cout << "Type Interval Init" << std::endl;
                    std::cout << ":";
                    std::string init;
                    std::cin >> init;

                    std::cout << "Type Interval End" << std::endl;
                    std::cout << ":";
                    std::string end;
                    std::cin >> end;

                    std::cout << "Type Animation Custom Name" << std::endl;
                    std::cout << ":";
                    std::string customname;
                    std::cin >> customname;

                    AssimpAnimationImporter* model = new AssimpAnimationImporter();
                    
                    int init1, end1;
                    std::stringstream(init) >> init1;
                    std::stringstream(end) >> end1;

                    if (model->Load(modelPath,init1,end1,customname))
                    {
                        if (model->ConvertToPyrosFormat(modelOutput + ".p3da"))
                            std::cout << "Converted to "<< modelOutput << ".p3da" << " - Press Enter to Continue..." << std::endl;
                        else std::cout << "Failed - Press Enter to Continue..." << std::endl;
                        cin.get(); 
                        cin.get();
                        delete model;
                    } else {
                        std::cout << "Failed to Import Model, Enter to Continue..." << std::endl;
                        cin.get(); 
                        cin.get();
                        delete model;
                        break;
                    }
                }
                break;
                case '4':
                {
                    showHelp();
                }
                break;
                case 'q':
                    init = false;
                break;
            }
        }
    }
    return 0;
}