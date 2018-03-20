from ngsolve import *
from netgen.csg import *
ngsglobals.msg_level = 0

def test_multiple_meshes_refine():
    mesh = Mesh(unit_cube.GenerateMesh(maxh=1))
    mesh.Refine()
    parents = [mesh.GetParentVertices(v) for v in range(mesh.nv)]
    print(parents)

    mesh2 = Mesh(unit_cube.GenerateMesh(maxh=0.5))
    mesh2.Refine()

    for v in range(mesh.nv):
        assert parents[v] == mesh.GetParentVertices(v)
