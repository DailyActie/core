#include <ph.h>
#include <phstream.h>
#include <phInput.h>
#include <phBC.h>
#include <phRestart.h>
#include <phAdapt.h>
#include <phOutput.h>
#include <phPartition.h>
#include <phFilterMatching.h>
#include <apfMDS.h>
#include <apfMesh2.h>
#include <apfPartition.h>
#include <apf.h>
#include <gmi_mesh.h>
#include <PCU.h>
#include <string>

#define SIZET(a) static_cast<size_t>(a)

namespace {

void freeMesh(apf::Mesh* m)
{
  m->destroyNative();
  apf::destroyMesh(m);
}

void afterSplit(apf::Mesh2* m, ph::Input& in, ph::Output& out,
    ph::BCs& bcs, int numMasters)
{
  std::string path = ph::setupOutputDir();
  ph::setupOutputSubdir(path);
  /* check if the mesh changed at all */
  if ((PCU_Comm_Peers()!=numMasters) ||
      in.adaptFlag ||
      in.tetrahedronize) {
    if (in.parmaPtn && PCU_Comm_Peers() > 1)
      ph::balance(m);
    apf::reorderMdsMesh(m);
  }
  ph::enterFilteredMatching(m, in, bcs);
  ph::generateOutput(in, bcs, m, out);
  ph::exitFilteredMatching(m);
  // a path is not needed for inmem
  ph::detachAndWriteSolution(in,out,m,path); //write restart
  ph::writeGeomBC(out, path); //write geombc
  ph::writeAuxiliaryFiles(path, in.timeStepNumber);
  if ( ! in.outMeshFileName.empty() )
    m->writeNative(in.outMeshFileName.c_str());
  m->verify();
}

void switchToMasters(int splitFactor)
{
  int self = PCU_Comm_Self();
  int groupRank = self / splitFactor;
  int group = self % splitFactor;
  MPI_Comm groupComm;
  MPI_Comm_split(MPI_COMM_WORLD, group, groupRank, &groupComm);
  PCU_Switch_Comm(groupComm);
}

void switchToAll()
{
  MPI_Comm prevComm = PCU_Get_Comm();
  PCU_Switch_Comm(MPI_COMM_WORLD);
  MPI_Comm_free(&prevComm);
  PCU_Barrier();
}

void loadCommon(ph::Input& in, ph::BCs& bcs, gmi_model*& g)
{
  ph::readBCs(in.attributeFileName.c_str(), bcs);
  if(!g)
    g = gmi_load(in.modelFileName.c_str());
}

void originalMain(apf::Mesh2*& m, ph::Input& in,
    gmi_model* g, apf::Migration*& plan)
{
  if(!m)
    m = apf::loadMdsMesh(g, in.meshFileName.c_str());
  else
    apf::printStats(m);
  m->verify();
  if (in.solutionMigration)
    ph::readAndAttachSolution(in, m);
  else
    ph::attachZeroSolution(in, m);
  if (in.buildMapping)
    ph::buildMapping(m);
  apf::setMigrationLimit(SIZET(in.elementsPerMigration));
  if (in.adaptFlag)
    ph::adapt(in, m);
  if (in.tetrahedronize)
    ph::tetrahedronize(in, m);
  plan = ph::split(in, m);
}

}//end namespace

namespace chef {
  static FILE* openfile_read(ph::Input&, const char* path) {
    fprintf(stderr, "------entering %s-------\n", __func__);
    return fopen(path, "r");
  }

  static FILE* openfile_write(ph::Output&, const char* path) {
    fprintf(stderr, "------entering %s-------\n", __func__);
    return fopen(path, "w");
  }

  static FILE* openstream_write(ph::Output& out, const char* path) {
    fprintf(stderr, "------entering %s-------\n", __func__);
    return openGRStreamWrite(out.grs, path);
  }

  static FILE* openstream_read(ph::Input& in, const char* path) {
    fprintf(stderr, "------entering %s-------\n", __func__);
    std::string fname(path);
    std::string restartStr("restart");
    FILE* f = NULL;
    if( fname.find(restartStr) != std::string::npos )
      f = openRStreamRead(in.rs);
    else {
      fprintf(stderr,
        "ERROR %s type of stream %s is unknown... exiting\n",
        __func__, fname.c_str());
      exit(1);
    }
    return f;
  }
  void bake(gmi_model*& g, apf::Mesh2*& m,
      ph::Input& in, ph::Output& out) {
    apf::Migration* plan = 0;
    ph::BCs bcs;
    loadCommon(in, bcs, g);
    const int worldRank = PCU_Comm_Self();
    switchToMasters(in.splitFactor);
    const int numMasters = PCU_Comm_Peers();
    if ((worldRank % in.splitFactor) == 0)
      originalMain(m, in, g, plan);
    switchToAll();
    if (in.adaptFlag)
      ph::goToStepDir(in.timeStepNumber);
    m = repeatMdsMesh(m, g, plan, in.splitFactor);
    afterSplit(m,in,out,bcs,numMasters);
    if (in.adaptFlag)
      ph::goToParentDir();
  }
  void cook(gmi_model*& g, apf::Mesh2*& m) {
    ph::Input in;
    in.openfile_read = openfile_read;
    ph::Output out;
    out.openfile_write = openfile_write;
    in.load("adapt.inp");
    bake(g,m,in,out);
  }
  void cook(gmi_model*& g, apf::Mesh2*& m,
      ph::Input& ctrl) {
    ctrl.openfile_read = openfile_read;
    ph::Output out;
    out.openfile_write = openfile_write;
    bake(g,m,ctrl,out);
  }
  void cook(gmi_model*& g, apf::Mesh2*& m,
      ph::Input& ctrl, GRStream* grs) {
    ctrl.openfile_read = openfile_read;
    ph::Output out;
    out.openfile_write = openstream_write;
    out.grs = grs;
    bake(g,m,ctrl,out);
  }
  void cook(gmi_model*& g, apf::Mesh2*& m,
      ph::Input& ctrl, RStream* rs) {
    ctrl.openfile_read = openstream_read;
    ctrl.rs = rs;
    ph::Output out;
    out.openfile_write = openfile_write;
    bake(g,m,ctrl,out);
    return;
  }
  void cook(gmi_model*& g, apf::Mesh2*& m,
      ph::Input& ctrl, RStream* rs, GRStream* grs) {
    ctrl.openfile_read = openstream_read;
    ctrl.rs = rs;
    ph::Output out;
    out.openfile_write = openstream_write;
    out.grs = grs;
    bake(g,m,ctrl,out);
    return;
  }
}