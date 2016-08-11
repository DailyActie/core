/****************************************************************************** 

  Copyright 2013 Scientific Computation Research Center, 
      Rensselaer Polytechnic Institute. All rights reserved.
  
  The LICENSE file included with this distribution describes the terms
  of the SCOREC Non-Commercial License this program is distributed under.
 
*******************************************************************************/
#ifndef PUMI_H
#define PUMI_H
#include <gmi.h>
#include <apfMesh2.h>

typedef gmi_model* pGeom;
typedef gmi_ent* pGeomEnt;
typedef apf::Mesh2* pMesh;
typedef apf::MeshEntity* pMeshEnt;
typedef apf::MeshEntity* pPartEnt;
typedef apf::MeshIterator* pMeshIter;
typedef apf::Copies Copies;
typedef apf::MeshTag* pTag;
typedef apf::Parts Parts;
typedef apf::EntityVector EntityVector;
typedef apf::Parts Parts;
typedef apf::Up Up;
typedef apf::Downward Downward;

// singleton to save model/mesh
class pumi
{
public:
  pumi();
  ~pumi();
  static pumi* instance();

  pMesh mesh;
  pGeom model;
private:
  static pumi* _instance;
};

/** \brief Distribution plan object: local elements to multiple destinations. */
// defined in pumi_distribution.cc
class Distribution
{
  public:
/** \brief must be constructed with a mesh
  \details use (new Distribution(mesh)) to make these objects */
    Distribution(pMesh m);
    ~Distribution();

/** \brief get the i'th element with an assigned destination */
    pMeshEnt get(int i);
/** \brief return true if the i'th element has been assigned destination(s) */
    bool has(pMeshEnt e);
/** \brief assign a destination part id to an element */
    void send(pMeshEnt e, int to);
/** \brief return the destination part id of an element */
    Parts& sending(pMeshEnt e);
    void print();
    int count();
    pMesh getMesh() {return mesh;}

    Parts* parts_vec;
    int element_count;
  private:
    pMesh mesh;
};

//************************************
//************************************
//      0- SYSTEM-LEVEL FUNCTIONS
//************************************
//************************************
void pumi_start();
void pumi_finalize(bool do_mpi_finalize=true);

int pumi_size();
int pumi_rank();

void pumi_sync(void);
void pumi_printsys();
double pumi_gettime();
double pumi_getmem();
void pumi_printtimemem(const char* msg, double time, double memory);

//************************************
//************************************
//      1- MESH FUNCTIONS
//************************************
//************************************

//************************************
// Model/Mesh management
//************************************

// create a model from a file
pGeom pumi_geom_load(const char* fileName, const char* model_type="mesh");

// load a serial mesh. 
pMesh pumi_mesh_loadserial(pGeom g, const char* filename, const char* mesh_type="mds");

// load a mesh from a file. Do static partitioning if num_in_part==1
pMesh pumi_mesh_load(pGeom geom, const char* fileName, int num_in_part, const char* mesh_type="mds");

// load a serial mesh on master process then distribute as per the distribution object
void pumi_mesh_distribute(pMesh m, Distribution* plan);

// get mesh dimension
int pumi_mesh_getdim(pMesh m);

// get # mesh entities of type d on local process
int pumi_mesh_getnument(pMesh m, int d);

// print mesh size info - global and local
void pumi_mesh_print(pMesh m);

// write mesh into a file - mesh_type should be "mds" or "vtk"
void pumi_mesh_write (pMesh m, const char* fileName, const char* mesh_type="mds");

// delete mesh
void pumi_mesh_delete(pMesh m);

// verify mesh
void pumi_mesh_verify(pMesh m);

//************************************
//  Ghosting
//************************************

/** \brief Ghosting plan object: local elements or part to destinations. */
// defined in pumi_ghost.cc
class Ghosting
{
  public:
/** \brief must be constructed with a mesh
  \details use (new apf::Migration(mesh)) to make these objects */
    Ghosting(pMesh, int);
    ~Ghosting();
/** \brief return the number of elements with ghost destinations */
    int count(int d);
/** \brief return the number of ghost destinations with entity */
    int count(pMeshEnt ent);
/** \brief get the i'th element with an assigned destination */
    pMeshEnt get(int d, int i);
/** \brief return true if the i'th element has been assigned a destination */
    bool has(pMeshEnt e);
/** \brief assign a destination part id to an entity */
    void send(pMeshEnt e, int to);
/** \brief assign a destination part id of all entities of dimension */
    void send(int dim, int to);
    void print();
/** \brief return the destination part id of an element */
//    int sending(pMeshEnt e);
    pMesh getMesh() {return m;}
    std::map<pMeshEnt, Parts> pid_map[4];
    int ghost_dim;
    pTag ghosted_tag;
    pTag ghost_tag;
  private:
    pMesh m;
};


/* 
input:
  - brgType - desired bridge entity type
  - ghostType - desired ghost entity type 
  - numLayer - the number of ghost layers
  - includeCopy - integer flag indicating whether to include non-owned bridge entity (1: yes, 0: no)
	If includeCopy is 0 and part boundary entity of brgType is not owned by a self part 
	(shortly, non-owned bridge type entity), ghost type dimensional entities adjacent to the non-owned
	bridge type entity is not ghost copied. If includeCopy is 1, all ghost type dimensional entities 
	adjacent to the bridge type entities on part boundaries are ghost copied.

The error is returned in the following cases:
  - bridge type is greater than or equal to ghost type
  - bridge type is greater than or equal to mesh dimension
  - ghost type is mesh vertex
  - ghost type is grester than mesh dimension
*/
pMesh pumi_ghost_createlayer (int brgType, int ghostType, int numLayer, int includeCopy);

/* 
  Ghosting: ghosting plan object for local elements or part to destinations.
*/
void pumi_ghost_create(pMesh m, Ghosting* plan);

// unavailable
void pumi_ghost_delete (pMesh m);

/* 
input: 	
  - a mesh instance
   
output: 
  - return the historical ghosting information in order which consists of four integers, ghost type, 
    bridge type, the number of layers, and includeCopy flag (0 or 1)

example:
  If pumi_ghost_create was called twice in the following order (mesh, 0, 2, 1, 1) and (mesh, 1, 3, 0), 
  the vector "ghostinfo" contains [0, 2, 1, 1, 1, 3, 3, 0]
*/
// unavailable
void pumi_ghost_info (pMesh m, std::vector<int>& ghostinfo);

//************************************
//  Mesh Entity
//************************************
// get geometric entity's dimension
int pumi_gent_getdim(pGeomEnt ge);

// get geometric entity's global id
int pumi_gent_getid(pGeomEnt ge);

void pumi_gent_getrevclas (pGeomEnt g, std::vector<pMeshEnt>& ents);

// get mesh entity's dimension
int pumi_ment_getdim(pMeshEnt e);

// get mesh entity's local id
int pumi_ment_getlocalid(pMeshEnt e);

// get mesh entity's global id - vertex only
// global id is maintained if mesh is ghosted
// global id is NOT maintained if mesh is adapted or re-partitioned
int pumi_ment_getglobalid(pMeshEnt e);

// get # adjacent entities
int pumi_ment_getnumadj(pMeshEnt e, int tgtType);

// get adjacent entities
void pumi_ment_getadj(pMeshEnt e, int tgtType, std::vector<pMeshEnt>& vecAdjEnt);

// get 2nd-order adjacent entities
void pumi_ment_get2ndadj (pMeshEnt e, int brgType, int tgtType, std::vector<pMeshEnt>& vecAdjEnt);

// return entity's geometric classification
pGeomEnt pumi_ment_getgeomclas(pMeshEnt e);

// unavailable
pPartEnt pumi_ment_getptnclas(pMeshEnt e);

// return owning part id. if ghosted mesh, vertex or element only
int pumi_ment_getownpid(pMeshEnt e); 

// return owner entity copy. if ghoted mesh, vertex or element only
pMeshEnt pumi_ment_getownent(pMeshEnt e); 

// return true if the entity is an owner copy
bool pumi_ment_isowned(pMeshEnt e);

// return true if entity is on part boundary, ghosted, or ghost
//  - this will fixed to consider only part boundary entities later
bool pumi_ment_isonbdry (pMeshEnt e); 

// return # remote and ghost copies
//  - this will fixed to consider only part boundary entities later
int pumi_ment_getnumrmt (pMeshEnt e); 

// return remote and ghost copies
//  - this will fixed to consider only part boundary entities later
void pumi_ment_getallrmt(pMeshEnt e, Copies& remotes); 

// return remote or ghost copy on a destination part
//  - this will fixed to consider only part boundary entities later
pMeshEnt pumi_ment_getrmt(pMeshEnt& meshEnt, int destPart); 

// unavailable
void pumi_ment_setrmt(pMeshEnt e, int partID, pMeshEnt rmtEnt);

// unavailable
void pumi_ment_deletermt (pMeshEnt e, int partID);

// unavailable
void pumi_ment_cleanrmt (pMeshEnt e);

// unavailable
void pumi_ment_setptntopology (pMeshEnt e);

// return part ids where the entity is duplicated - part boundary or ghost
void pumi_ment_getresidence(pMeshEnt e, std::vector<int>& resPartId);

// return part ids where the entity and its downward adjacency are duplicated - part boundary or ghost
void pumi_ment_getclosureresidence(pMeshEnt ent, std::vector<int>& resPartId);

// return true if the entity is a ghost copy
bool pumi_ment_isghost(pMeshEnt e);

// return true if the entity is ghosted
// unavailable
bool pumi_ment_isghosted (pMeshEnt e);

// unavailable
// return #ghost copies
int pumi_ment_getnumghost (pMeshEnt e);

// unavailable
// return ghost copies
void pumi_ment_getallghost (pMeshEnt e, Copies&);

// unavailable
// return ghost copy on a destination part
pMeshEnt pumi_ment_getghost(pMeshEnt& e, int partID);
#endif
