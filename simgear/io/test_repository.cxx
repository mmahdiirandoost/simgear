#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <errno.h>
#include <fcntl.h>

#include <boost/algorithm/string/case_conv.hpp>

#include <simgear/simgear_config.h>

#include "test_HTTP.hxx"
#include "HTTPRepository.hxx"
#include "HTTPClient.hxx"

#include <simgear/misc/strutils.hxx>
#include <simgear/misc/sg_hash.hxx>
#include <simgear/timing/timestamp.hxx>
#include <simgear/debug/logstream.hxx>
#include <simgear/misc/sg_dir.hxx>
#include <simgear/structure/exception.hxx>

using namespace simgear;

std::string dataForFile(const std::string& parentName, const std::string& name, int revision)
{
    std::ostringstream os;
    // random content but which definitely depends on our tree location
    // and revision.
    for (int i=0; i<100; ++i) {
        os << i << parentName << "_" << name << "_" << revision;
    }

    return os.str();
}

class TestRepoEntry
{
public:
    TestRepoEntry(TestRepoEntry* parent, const std::string& name, bool isDir);
    ~TestRepoEntry();

    TestRepoEntry* parent;
    std::string name;

    std::string indexLine() const;

    std::string hash() const;

    std::vector<TestRepoEntry*> children;

    size_t sizeInBytes() const
    {
        return data().size();
    }

    bool isDir;
    int revision; // for files
    int requestCount;

    void clearRequestCounts();

    std::string pathInRepo() const
    {
        return parent ? (parent->pathInRepo() + "/" + name) : name;
    }

    std::string data() const;

    void defineFile(const std::string& path, int rev = 1)
    {
        string_list pathParts = strutils::split(path, "/");
        if (pathParts.size() == 1) {
            children.push_back(new TestRepoEntry(this, pathParts.front(), false));
            children.back()->revision = rev;
        } else {
            // recurse
            TestRepoEntry* c = childEntry(pathParts.front());
            if (!c) {
                // define a new directory child
                c = new TestRepoEntry(this, pathParts.front(), true);
                children.push_back(c);
            }

            size_t frontPartLength = pathParts.front().size();
            c->defineFile(path.substr(frontPartLength + 1), rev);
        }
    }

    TestRepoEntry* findEntry(const std::string& path)
    {
        if (path.empty()) {
            return this;
        }
        
        string_list pathParts = strutils::split(path, "/");
        TestRepoEntry* entry = childEntry(pathParts.front());
        if (pathParts.size() == 1) {
            return entry; // might be NULL
        }

        if (!entry) {
            std::cerr << "bad path: " << path << std::endl;
            return NULL;
        }

        size_t part0Length = pathParts.front().size() + 1;
        return entry->findEntry(path.substr(part0Length));
    }

    TestRepoEntry* childEntry(const std::string& name) const
    {
        assert(isDir);
        for (size_t i=0; i<children.size(); ++i) {
            if (children[i]->name == name) {
                return children[i];
            }
        }

        return NULL;
    }

    void removeChild(const std::string& name)
    {
        std::vector<TestRepoEntry*>::iterator it;
        for (it = children.begin(); it != children.end(); ++it) {
            if ((*it)->name == name) {
                delete *it;
                children.erase(it);
                return;
            }
        }
        std::cerr << "child not found:" << name << std::endl;
    }
};

TestRepoEntry::TestRepoEntry(TestRepoEntry* pr, const std::string& nm, bool d) :
    parent(pr), name(nm), isDir(d)
{
    revision = 2;
    requestCount = 0;
}

TestRepoEntry::~TestRepoEntry()
{
    for (size_t i=0; i<children.size(); ++i) {
        delete children[i];
    }
}

std::string TestRepoEntry::data() const
{
    if (isDir) {
        std::ostringstream os;
        os << "version:1\n";
        os << "path:" << pathInRepo() << "\n";
        for (size_t i=0; i<children.size(); ++i) {
            os << children[i]->indexLine() << "\n";
        }
        return os.str();
    } else {
        return dataForFile(parent->name, name, revision);
    }
}

std::string TestRepoEntry::indexLine() const
{
    std::ostringstream os;
    os << (isDir ? "d:" : "f:") << name << ":" << hash()
        << ":" << sizeInBytes();
    return os.str();
}

std::string TestRepoEntry::hash() const
{
    simgear::sha1nfo info;
    sha1_init(&info);
    std::string d(data());
    sha1_write(&info, d.data(), d.size());
    return strutils::encodeHex(sha1_result(&info), HASH_LENGTH);
}

void TestRepoEntry::clearRequestCounts()
{
    requestCount = 0;
    if (isDir) {
        for (size_t i=0; i<children.size(); ++i) {
            children[i]->clearRequestCounts();
        }
    }
}

TestRepoEntry* global_repo = NULL;

class TestRepositoryChannel : public TestServerChannel
{
public:

    virtual void processRequestHeaders()
    {
        state = STATE_IDLE;
        if (path.find("/repo/") == 0) {
//            std::cerr << "get for:" << path << std::endl;

            std::string repoPath = path.substr(6);
            bool lookingForDir = false;
            std::string::size_type suffix = repoPath.find(".dirindex");
            if (suffix != std::string::npos) {
                lookingForDir = true;
                if (suffix > 0) {
                    // trim the preceeding '/' as well, for non-root dirs
                    suffix--;
                }

                repoPath = repoPath.substr(0, suffix);
            }

            TestRepoEntry* entry = global_repo->findEntry(repoPath);
            if (!entry) {
                sendErrorResponse(404, false, "unknown repo path:" + repoPath);
                return;
            }

            if (entry->isDir != lookingForDir) {
                sendErrorResponse(404, false, "mismatched path type:" + repoPath);
                return;
            }

            entry->requestCount++;
            std::string content(entry->data());
            std::stringstream d;
            d << "HTTP/1.1 " << 200 << " " << reasonForCode(200) << "\r\n";
            d << "Content-Length:" << content.size() << "\r\n";
            d << "\r\n"; // final CRLF to terminate the headers
            d << content;
            push(d.str().c_str());
        } else {
            sendErrorResponse(404, false, "");
        }
    }
};

std::string test_computeHashForPath(const SGPath& p)
{
    if (!p.exists())
        return std::string();
    sha1nfo info;
    sha1_init(&info);
    char* buf = static_cast<char*>(alloca(1024 * 1024));
    size_t readLen;
    int fd = ::open(p.c_str(), O_RDONLY);
    while ((readLen = ::read(fd, buf, 1024 * 1024)) > 0) {
        sha1_write(&info, buf, readLen);
    }

    std::string hashBytes((char*) sha1_result(&info), HASH_LENGTH);
    return strutils::encodeHex(hashBytes);
}

void verifyFileState(const SGPath& fsRoot, const std::string& relPath)
{
    TestRepoEntry* entry = global_repo->findEntry(relPath);
    if (!entry) {
        throw sg_error("Missing test repo entry", relPath);
    }

    SGPath p(fsRoot);
    p.append(relPath);
    if (!p.exists()) {
        throw sg_error("Missing file system entry", relPath);
    }

    std::string hashOnDisk = test_computeHashForPath(p);
    if (hashOnDisk != entry->hash()) {
        throw sg_error("Checksum mismatch", relPath);
    }
}

void verifyRequestCount(const std::string& relPath, int count)
{
    TestRepoEntry* entry = global_repo->findEntry(relPath);
    if (!entry) {
        throw sg_error("Missing test repo entry", relPath);
    }

    if (entry->requestCount != count) {
        throw sg_exception("Bad request count", relPath);
    }
}

void createFile(const SGPath& basePath, const std::string& relPath, int revision)
{
    string_list comps = strutils::split(relPath, "/");

    SGPath p(basePath);
    p.append(relPath);

    simgear::Dir d(p.dir());
    d.create(0700);

    std::string prName = comps.at(comps.size() - 2);
    {
        std::ofstream f(p.c_str(), std::ios::trunc | std::ios::out);
        f << dataForFile(prName, comps.back(), revision);
    }
}

TestServer<TestRepositoryChannel> testServer;

void waitForUpdateComplete(HTTP::Client* cl, HTTPRepository* repo)
{
    SGTimeStamp start(SGTimeStamp::now());
    while (start.elapsedMSec() <  10000) {
        cl->update();
        testServer.poll();

        if (!repo->isDoingSync()) {
            return;
        }
        SGTimeStamp::sleepForMSec(15);
    }

    std::cerr << "timed out" << std::endl;
}

void testBasicClone(HTTP::Client* cl)
{
    std::auto_ptr<HTTPRepository> repo;
    SGPath p(simgear::Dir::current().path());
    p.append("http_repo_basic");
    simgear::Dir pd(p);
    pd.removeChildren();
    
    repo.reset(new HTTPRepository(p, cl));
    repo->setBaseUrl("http://localhost:2000/repo");
    repo->update();

    waitForUpdateComplete(cl, repo.get());

    verifyFileState(p, "fileA");
    verifyFileState(p, "dirA/subdirA/fileAAA");
    verifyFileState(p, "dirC/subdirA/subsubA/fileCAAA");

    global_repo->findEntry("fileA")->revision++;
    global_repo->findEntry("dirB/subdirA/fileBAA")->revision++;
    global_repo->defineFile("dirC/fileCA"); // new file
    global_repo->findEntry("dirB/subdirA")->removeChild("fileBAB");
    global_repo->findEntry("dirA")->removeChild("subdirA"); // remove a dir

    repo->update();

    // verify deltas
    waitForUpdateComplete(cl, repo.get());

    verifyFileState(p, "fileA");
    verifyFileState(p, "dirC/fileCA");

    std::cout << "Passed test: basic clone and update" << std::endl;
}

void testModifyLocalFiles(HTTP::Client* cl)
{
    std::auto_ptr<HTTPRepository> repo;
    SGPath p(simgear::Dir::current().path());
    p.append("http_repo_modify_local_2");
    simgear::Dir pd(p);
    if (pd.exists()) {
        pd.removeChildren();
    }

    repo.reset(new HTTPRepository(p, cl));
    repo->setBaseUrl("http://localhost:2000/repo");
    repo->update();

    waitForUpdateComplete(cl, repo.get());
    verifyFileState(p, "dirB/subdirA/fileBAA");

    SGPath modFile(p);
    modFile.append("dirB/subdirA/fileBAA");
    {
        std::ofstream of(modFile.c_str(), std::ios::out | std::ios::trunc);
        of << "complete nonsense";
        of.close();
    }

    global_repo->clearRequestCounts();
    repo->update();
    waitForUpdateComplete(cl, repo.get());
    verifyFileState(p, "dirB/subdirA/fileBAA");
    verifyRequestCount("dirB", 0);
    verifyRequestCount("dirB/subdirA", 0);
    verifyRequestCount("dirB/subdirA/fileBAA", 1);

    std::cout << "Passed test: identify and fix locally modified files" << std::endl;
}

void testNoChangesUpdate()
{

}

void testMergeExistingFileWithoutDownload(HTTP::Client* cl)
{
    std::auto_ptr<HTTPRepository> repo;
    SGPath p(simgear::Dir::current().path());
    p.append("http_repo_merge_existing");
    simgear::Dir pd(p);
    if (pd.exists()) {
        pd.removeChildren();
    }

    repo.reset(new HTTPRepository(p, cl));
    repo->setBaseUrl("http://localhost:2000/repo");

    createFile(p, "dirC/fileCB", 4); // should match
    createFile(p, "dirC/fileCC", 3); // mismatch

    global_repo->defineFile("dirC/fileCB", 4);
    global_repo->defineFile("dirC/fileCC", 10);

    // new sub-tree
    createFile(p, "dirD/fileDA", 4);
    createFile(p, "dirD/subdirDA/fileDAA", 6);
    createFile(p, "dirD/subdirDB/fileDBA", 6);

    global_repo->defineFile("dirD/fileDA", 4);
    global_repo->defineFile("dirD/subdirDA/fileDAA", 6);
    global_repo->defineFile("dirD/subdirDB/fileDBA", 6);

    repo->update();
    waitForUpdateComplete(cl, repo.get());
    verifyFileState(p, "dirC/fileCB");
    verifyFileState(p, "dirC/fileCC");
    verifyRequestCount("dirC/fileCB", 0);
    verifyRequestCount("dirC/fileCC", 1);

    verifyRequestCount("dirD/fileDA", 0);
    verifyRequestCount("dirD/subdirDA/fileDAA", 0);
    verifyRequestCount("dirD/subdirDB/fileDBA", 0);

    std::cout << "Passed test: merge existing files with matching hash" << std::endl;
}

void testLossOfLocalFiles(HTTP::Client* cl)
{
    std::auto_ptr<HTTPRepository> repo;
    SGPath p(simgear::Dir::current().path());
    p.append("http_repo_lose_local");
    simgear::Dir pd(p);
    if (pd.exists()) {
        pd.removeChildren();
    }

    repo.reset(new HTTPRepository(p, cl));
    repo->setBaseUrl("http://localhost:2000/repo");
    repo->update();
    waitForUpdateComplete(cl, repo.get());
    verifyFileState(p, "dirB/subdirA/fileBAA");

    SGPath lostPath(p);
    lostPath.append("dirB/subdirA");
    simgear::Dir lpd(lostPath);
    lpd.remove(true);

    global_repo->clearRequestCounts();

    repo->update();
    waitForUpdateComplete(cl, repo.get());
    verifyFileState(p, "dirB/subdirA/fileBAA");

    verifyRequestCount("dirB", 0);
    verifyRequestCount("dirB/subdirA", 1);
    verifyRequestCount("dirB/subdirA/fileBAC", 1);

    std::cout << "Passed test: lose and replace local files" << std::endl;
}

int main(int argc, char* argv[])
{
  sglog().setLogLevels( SG_ALL, SG_INFO );

  HTTP::Client cl;
  cl.setMaxConnections(1);

    global_repo = new TestRepoEntry(NULL, "root", true);
    global_repo->defineFile("fileA");
    global_repo->defineFile("fileB");
    global_repo->defineFile("dirA/fileAA");
    global_repo->defineFile("dirA/fileAB");
    global_repo->defineFile("dirA/fileAC");
    global_repo->defineFile("dirA/subdirA/fileAAA");
    global_repo->defineFile("dirA/subdirA/fileAAB");
    global_repo->defineFile("dirB/subdirA/fileBAA");
    global_repo->defineFile("dirB/subdirA/fileBAB");
    global_repo->defineFile("dirB/subdirA/fileBAC");
    global_repo->defineFile("dirC/subdirA/subsubA/fileCAAA");

    testBasicClone(&cl);

    testModifyLocalFiles(&cl);

    testLossOfLocalFiles(&cl);

    testMergeExistingFileWithoutDownload(&cl);

    return 0;
}