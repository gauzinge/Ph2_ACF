#include "FileHandler.h"

//Constructor
FileHandler::FileHandler ( const std::string& pBinaryFileName, char pOption ) :
    fBinaryFileName ( pBinaryFileName ),
    fOption ( pOption ),
    fFileIsOpened ( false )
{
    openFile();

    if ( fOption == 'w' )
    {
        fThread = std::thread ( &FileHandler::writeFile, this );
        fThread.detach();
    }
}

FileHandler::FileHandler ( const std::string& pBinaryFileName, char pOption, FileHeader pHeader ) :
    fBinaryFileName ( pBinaryFileName ),
    fOption ( pOption ),
    fFileIsOpened ( false ),
    fHeader ( pHeader )
{
    openFile();

    if ( fOption == 'w' )
    {
        fThread = std::thread ( &FileHandler::writeFile, this );
        fThread.detach();
    }
}

//destructor
FileHandler::~FileHandler()
{
    if (fOption == 'w' && fThread.joinable() )
        fThread.join();

    closeFile();
}

void FileHandler::set ( std::vector<uint32_t> pVector )
{
    //fMutex.lock();
    //fData.clear();
    //fData = pVector;
    //is_set = true;

    //if ( is_set )
    //fMutex.unlock();

    std::lock_guard<std::mutex> cLock (fMutex);
    fQueue.push (pVector);
    fSet.notify_one();
}

bool FileHandler::openFile( )
{

    if ( !file_open() )
    {
        std::lock_guard<std::mutex> cLock (fMutex);

        if ( fOption == 'w' )
        {
            fBinaryFile.open ( ( getFilename() ).c_str(), std::fstream::trunc | std::fstream::out | std::fstream::binary );

            // if the header is null or not valid, continue without and delete the header
            if ( fHeader.fValid == false )
                LOG (INFO) << "FileHandler: Warning - No valid file Header provided, writing file without ... " ;
            //if the header object is valid i serialize it in the file
            else if ( fHeader.fValid)
            {
                std::vector<uint32_t> cHeaderVec = fHeader.encodeHeader();
                uint32_t cBuffer[cHeaderVec.size()];
                std::copy ( cHeaderVec.begin(), cHeaderVec.end(), cBuffer );
                fBinaryFile.write ( ( char* ) &cBuffer, sizeof ( cBuffer ) );
            }
        }

        else if ( fOption == 'r' )
        {

            fBinaryFile.open ( getFilename().c_str(),  std::fstream::in |  std::fstream::binary );

            // read the first 12 words and check if it is header
            // if yes, everything cool

            //now I can try to decode the header and check if it is valid
            fHeader.decodeHeader ( this->readFileChunks (fHeader.fHeaderSize32) );

            // if the header is not valid, return to the beginning of the fiel
            // and treat it as normal data
            if (!fHeader.fValid)
            {
                LOG (INFO) << "FileHandler: No valid header found in file " << fBinaryFileName << " - resetting to 0 and treating as normal data!" ;
                fBinaryFile.clear( );
                fBinaryFile.seekg ( 0, std::ios::beg );
                // if the file Header is nullptr I do not get info from it!
            }
            else LOG (INFO) << "FileHandler: Found a valid header in file " << fBinaryFileName ;
        }

        fFileIsOpened = true;
    }

    return file_open();
}

void FileHandler::closeFile()
{
    std::lock_guard<std::mutex> cLock (fMutex);

    if (fFileIsOpened)
    {
        fBinaryFile.close();
        fFileIsOpened = false;
    }
}

//read from raw file to vector
std::vector<uint32_t> FileHandler::readFile( )
{
    std::vector<uint32_t> cVector;

    //open file for reading
    while ( !fBinaryFile.eof() )
    {
        char buffer[4];
        fBinaryFile.read ( buffer, 4 );
        uint32_t word;
        std::memcpy ( &word, buffer, 4 );
        cVector.push_back ( word );
    }

    closeFile();
    return cVector;
}

//read from raw file to vector in chunks of pNWords32 32-bit words
std::vector<uint32_t> FileHandler::readFileChunks ( uint32_t pNWords32 )
{
    std::vector<uint32_t> cVector;
    uint32_t cWordCounter = 0;

    //open file for reading
    while (!fBinaryFile.eof() && cWordCounter < pNWords32)
    {
        char buffer[4];
        fBinaryFile.read ( buffer, 4 );
        uint32_t word;
        std::memcpy ( &word, buffer, 4 );
        cVector.push_back ( word );
        cWordCounter++;
    }

    if (fBinaryFile.eof() )
        closeFile();

    if (cWordCounter < pNWords32) LOG (INFO) << "FileHandler: Attention, input file " << fBinaryFileName << " ended before reading " << pNWords32 << " 32-bit words!" ;

    return cVector;
}

std::vector<uint32_t> FileHandler::readFileTail ( long pNbytes )
{
    // if pNbytes > -1 read only the last pNbytes words
    if (pNbytes > -1)
    {
        fBinaryFile.seekp (0, std::ios::end); // go to the end of the file
        fBinaryFile.seekp (-pNbytes, std::ios::cur); // back up n bytes
    }

    std::vector<uint32_t> cVector;

    //open file for reading
    while ( !fBinaryFile.eof() )
    {
        char buffer[4];
        fBinaryFile.read ( buffer, 4 );
        uint32_t word;
        std::memcpy ( &word, buffer, 4 );
        cVector.push_back ( word );
    }

    closeFile();
    return cVector;
}

void FileHandler::writeFile()
{
    //while ( true ) {
    //if ( is_set )
    //{
    //fMutex.lock();
    //uint32_t cBuffer[fData.size()];
    //std::copy ( fData.begin(), fData.end(), cBuffer );
    //fMutex.unlock();
    //fBinaryFile.write ( ( char* ) &cBuffer, sizeof ( cBuffer ) );
    //fBinaryFile.flush();
    //fData.clear();
    //is_set = false;
    ////continue;
    //}

    //else
    //{
    //fMutex.lock();
    //fMutex.unlock();
    ////continue;
    //}

    //}

    //new implementation using queue
    //this needs to run in an infinite loop, otherwise it will end after the first data was processed and the second on is not ready I think
    //anyway, dequeue will block this thread as long as fQueue is empty, if it is not, the first element will immediately be extracted
    while (true)
    {
        // a local data handle
        std::vector<uint32_t> cData;
        //populate the local handle with values from the queue -
        //this method blocks this thread until it receives data
        this->dequeue (cData);
        //copy data in buffer array for faster I/O
        uint32_t cBuffer[cData.size()];
        std::copy ( cData.begin(), cData.end(), cBuffer );
        //write the buffer
        fBinaryFile.write ( ( char* ) &cBuffer, sizeof ( cBuffer ) );
        fBinaryFile.flush();
    }

}

void FileHandler::dequeue (std::vector<uint32_t>& pData)
{
    std::unique_lock<std::mutex> cLock (fMutex);

    while (fQueue.empty() )
        fSet.wait (cLock);

    pData = fQueue.front();
    fQueue.pop();
}
