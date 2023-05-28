#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <cstring>
#include <dirent.h>

int numberOfVideo;
int *processIDs;

struct Video
{
    char timestamp[9];
    char *title;
} *videos;

/**
 * @param filename the file name
 * @return pointer to the extension or null if something wrong with the name
 */
char *getFileExtension(char *filename)
{
    char *dot = strrchr(filename, '.');
    if (dot == nullptr || dot == filename)
        return nullptr;
    return dot + 1;
}

void waitForVideoProcess()
{
    int status;
    int id = wait(&status);
    if (WIFEXITED(status))
    {
        if (id < 1)
            return;
        for (int i = 0; i < numberOfVideo; i++)
        {
            if (id == processIDs[i])
            {
                processIDs[i] = -1;
                int statusCode = WEXITSTATUS(status);
                if (statusCode == 0)
                    std::cout << "process " << videos[i].title << " is done" << std::endl;
                else
                    std::cout << "process " << videos[i].title << " failed with " << statusCode << " FFMPEG status code" << std::endl;
            }
        }
    }
}

int main(int argc, char *argv[])
{
    // check number if arguments
    if (argc < 4)
    {
        std::cout << "Expected 3 arguments " << std::endl
                  << "video file" << std::endl
                  << "content file" << std::endl
                  << "output dir" << std::endl;
        return 1;
    }
    // check if video file exist
    if (access(argv[1], F_OK) != 0)
    {
        std::cout << "video file do not exist" << std::endl;
        return 2;
    }
    // check if video is mp4 of avi
    if (strcmp(getFileExtension(argv[1]), "mp4") != 0 && strcmp(getFileExtension(argv[1]), "avi") != 0)
    {
        std::cout << "video file should be mp4 or avi file" << std::endl;
        return 3;
    }
    // check if content file exist
    if (access(argv[2], F_OK) != 0)
    {
        std::cout << "content file do not exist" << std::endl;
        return 4;
    }
    // check if content file is txt
    if (getFileExtension(argv[2]) == nullptr || strcmp(getFileExtension(argv[2]), "txt") != 0)
    {
        std::cout << "content file should be txt file" << std::endl;
        return 5;
    }
    // check if output dir exist
    DIR *dir = opendir(argv[3]);
    if (dir == nullptr)
    {
        std::cout << "output dir do not exist" << std::endl;
        return 6;
    }

    // check if output dir is empty
    int n = 0;
    while (readdir(dir) != nullptr)
    {
        if (++n > 2)
            break;
    }
    closedir(dir);
    if (!(n <= 2))
    {
        std::cout << "output dir is not empty" << std::endl;
        return 7;
    }

    int readFormParserPipe[2];
    int writeToParserPipe[2];
    pipe(readFormParserPipe);
    pipe(writeToParserPipe);

    int parserProcess = fork();
    if (parserProcess == 0)
    {
        // Change STDIN and STDOUT to our pipe in the parser process
        dup2(writeToParserPipe[0], STDIN_FILENO);
        close(writeToParserPipe[1]);
        close(writeToParserPipe[0]);
        dup2(readFormParserPipe[1], STDOUT_FILENO);
        close(readFormParserPipe[0]);
        close(readFormParserPipe[1]);

        execlp("node", "node", "./parser", nullptr);
        std::cout << "failed to run parser" << std::endl;
        return 0;
    }

    // send content to parser
    char *massage = new char[255];
    strcpy(massage, argv[2]);
    strcat(massage, "\n");
    close(writeToParserPipe[0]);
    if (write(writeToParserPipe[1], massage, strlen(massage)) == -1)
    {
        std::cout << "failed to communicate with the parser" << std::endl;
        return 8;
    }
    close(writeToParserPipe[1]);
    close(readFormParserPipe[1]);
    delete[] massage;

    // recive the number of parts
    if (read(readFormParserPipe[0], &numberOfVideo, sizeof(int)) == -1)
    {
        std::cout << "failed to communicate with the parser" << std::endl;
        return 9;
    }
    videos = new Video[numberOfVideo];
    processIDs = new int[numberOfVideo];

    for (int i = 0; i < numberOfVideo; i++)
    {
        // recive video timestamp
        if (read(readFormParserPipe[0], &videos[i].timestamp, 9) == -1)
        {
            std::cout << "failed to communicate with the parser" << std::endl;
            return 10;
        }

        // revive video title
        int titleLength;
        if (read(readFormParserPipe[0], &titleLength, sizeof(int)) == -1)
        {
            std::cout << "failed to communicate with the parser" << std::endl;
            return 11;
        }
        char *title = new char[titleLength];
        if (read(readFormParserPipe[0], title, titleLength) == -1)
        {
            std::cout << "failed to communicate with the parser" << std::endl;
            return 12;
        }
        videos[i].title = title;
    }
    close(readFormParserPipe[0]);

    // get number of CPU cores
    int CPUCores = sysconf(_SC_NPROCESSORS_ONLN);
    CPUCores = ((CPUCores > 0) ? CPUCores : 1);

    int FFMPEGProcesses = 0;

    // output dir path
    char *outputDir = new char[255];
    strcpy(outputDir, argv[3]);
    if (argv[3][strlen(argv[3]) - 1] != '/')
        strcat(outputDir, "/");

    // info.txt path
    char *outputInfoPath = new char[255];
    strcpy(outputInfoPath, outputDir);
    strcat(outputInfoPath, "Info.txt");

    int outputInfo = open(outputInfoPath, O_WRONLY | O_CREAT, 0777);
    for (int i = 0; i < numberOfVideo; i++)
    {
        /*
            if the number of process is more then CPU cores
            stop making new process until one finishes
        */
        if (FFMPEGProcesses >= CPUCores)
        {
            waitForVideoProcess();
            FFMPEGProcesses--;
        }

        // create FFMPEG process
        if ((processIDs[i] = fork()) < 0)
        {
            std::cout << "unable to create FFMPEG process for " << videos[i].title << std::endl;
        }
        else if (processIDs[i] == 0)
        {
            // make output path
            char *output = new char[255];
            strcpy(output, outputDir);
            strcat(output, videos[i].title);

            // redirect output if the process to info.txt file
            dup2(outputInfo, STDOUT_FILENO);
            dup2(outputInfo, STDERR_FILENO);
            close(outputInfo);

            // the following checks is to make cutting accurate

            // if first part
            if (i != 0 && i != (numberOfVideo - 1))
                execlp("ffmpeg", "ffmpeg", "-i", argv[1], "-ss", videos[i].timestamp, "-to", videos[i + 1].timestamp, "-c", "copy", output, nullptr);
            else if (i == 0)
                execlp("ffmpeg", "ffmpeg", "-ss", videos[i].timestamp, "-i", argv[1], "-to", videos[i + 1].timestamp, "-c", "copy", output, nullptr);
            // if last part
            else
                execlp("ffmpeg", "ffmpeg", "-ss", videos[i].timestamp, "-i", argv[1], "-c", "copy", strcat(argv[3], videos[i].title), nullptr);
            std::cout << "unable to run FFMPEG command" << std::endl;
            delete[] output;
            return 0;
        }
        else
        {
            FFMPEGProcesses++;
        }
    }

    // get the number of yet unfinished process
    int unfinishedProcesses = 0;
    for (int i = 0; i < numberOfVideo; i++)
        if (processIDs[i] != -1)
            unfinishedProcesses++;

    // waiting for unfinished process
    for (int i = 0; i < unfinishedProcesses; i++)
        waitForVideoProcess();
}