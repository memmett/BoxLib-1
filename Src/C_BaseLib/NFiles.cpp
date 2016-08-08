
#include <Utility.H>
#include <NFiles.H>


NFilesIter::NFilesIter(int noutfiles, const std::string &filePrefix,
                       bool groupsets, bool setBuf)
{
  groupSets = groupsets;
  myProc    = ParallelDescriptor::MyProc();
  nProcs    = ParallelDescriptor::NProcs();
  nOutFiles = ActualNFiles(noutfiles);
  nSets     = NSets(nProcs, nOutFiles);
  if(groupSets) {
    mySet     = myProc / nOutFiles;
  } else {
    mySet     = myProc % nSets;
  }
  int fileNumber(FileNumber(nOutFiles, myProc, groupSets));
  fullFileName  = BoxLib::Concatenate(filePrefix, fileNumber, 5);


  finishedWriting = false;

  if(setBuf) {
    io_buffer.resize(VisMF::IO_Buffer_Size);
    fileStream.rdbuf()->pubsetbuf(io_buffer.dataPtr(), io_buffer.size());
  }

}


NFilesIter::~NFilesIter() {
}


bool NFilesIter::ReadyToWrite() {

  if(finishedWriting) {
    return false;
  }

  for(int iSet(0); iSet < nSets; ++iSet) {
    if(mySet == iSet) {
      if(iSet == 0) {   // ---- first set
        fileStream.open(fullFileName.c_str(),
                        std::ios::out | std::ios::trunc | std::ios::binary);
      } else {
        fileStream.open(fullFileName.c_str(),
                        std::ios::out | std::ios::app | std::ios::binary);
        fileStream.seekp(0, std::ios::end);   // ---- set to eof
      }
      if( ! fileStream.good()) {
        BoxLib::FileOpenFailed(fullFileName);
      }
      return true;
    }

    if(mySet == (iSet + 1)) {   // ---- next set waits
      int iBuff, waitForPID(-1), tag(-2);
      if(groupSets) {
        waitForPID = (myProc - nOutFiles);
        tag = (myProc % nOutFiles);
      } else {
        waitForPID = (myProc - 1);
        tag = (myProc / nSets);
      }
      ParallelDescriptor::Recv(&iBuff, 1, waitForPID, tag);
    }
  }
  return false;
}


NFilesIter &NFilesIter::operator++() {
  fileStream.flush();
  fileStream.close();

  int iBuff(0), wakeUpPID(-1), tag(-2);
  if(groupSets) {
    wakeUpPID = (myProc + nOutFiles);
    tag = (myProc % nOutFiles);
  } else {
    wakeUpPID = (myProc + 1);
    tag = (myProc / nSets);
  }
  if(wakeUpPID < nProcs) {
    ParallelDescriptor::Send(&iBuff, 1, wakeUpPID, tag);
  }
  finishedWriting = true;
  return *this;
}


std::streampos NFilesIter::SeekPos() {
  return fileStream.tellp();
}

