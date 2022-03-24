#include "Mesh3D.hh"

#include <typed-geometry/tg.hh>

#include <fstream>

#include <glow/common/log.hh>
#include <glow/objects/ArrayBuffer.hh>
#include <glow/objects/ElementArrayBuffer.hh>
#include <glow/objects/VertexArray.hh>

#include <polymesh/Mesh.hh>
#include <polymesh/algorithms/triangulate.hh>
#include <polymesh/fields.hh>
#include <polymesh/formats/obj.hh>
#include <polymesh/properties.hh>

bool Mesh3D::loadFromFile(const std::string& filename, bool invertUvV, bool interpolate_tangents)
{
    if (!std::ifstream(filename).good())
    {
        glow::error() << filename << " cannot be opened";
        return false;
    }

    pm::obj_reader<float> obj_reader(filename, *this);
    if (obj_reader.error_faces() > 0)
    {
        glow::error() << filename << " contains " << obj_reader.error_faces() << " faces that could not be added";
        return false;
    }

    normal = obj_reader.get_normals().to<tg::vec3>();
    texCoord = obj_reader.get_tex_coords().to<tg::pos2>();
    position = obj_reader.get_positions().to<tg::pos3>();
    tangent = halfedges().make_attribute(tg::vec3(0, 0, 0));
    auto interpolated_tangents = vertices().make_attribute(tg::vec3(0, 0, 0));

    auto mesh_has_texcoords = obj_reader.has_valid_texcoords();
    auto mesh_has_normals = obj_reader.has_valid_normals();

    if (!mesh_has_normals)
    {
        // Compute normals
        auto vnormals = pm::vertex_normals_by_area(position);
        for (auto h : halfedges())
            normal[h] = vnormals[h.vertex_to()];
    }

    if (!mesh_has_texcoords)
    {
        glow::warning() << "Mesh " << filename << " does not have texture coordinates. Cannot compute tangents.";
        return false;
    }
    if (invertUvV)
    {
        texCoord = texCoord.map([&](tg::pos2 uv) { return tg::pos2(uv.x, 1-uv.y); });
    }

    // creation of tangents for triangle meshes
    auto faceTangents = faces().make_attribute<tg::vec3>();
    for (auto f : faces())
    {
        tg::pos3 p[3];
        tg::pos2 t[3];
        int cnt = 0;
        for (auto h : f.halfedges())
        {
            if (cnt >= 3)
                break;

            auto v = h.vertex_to();
            tg::pos3& pos = position[v];
            p[cnt] = pos;
            minExtents.x = tg::min(pos.x, minExtents.x);
            minExtents.y = tg::min(pos.y, minExtents.y);
            minExtents.z = tg::min(pos.z, minExtents.z);
            maxExtents.x = tg::max(pos.x, maxExtents.x);
            maxExtents.y = tg::max(pos.y, maxExtents.y);
            maxExtents.z = tg::max(pos.z, maxExtents.z);
            t[cnt] = texCoord[h];
            ++cnt;
        }

        auto p10 = p[1] - p[0];
        auto p20 = p[2] - p[0];

        auto t10 = t[1] - t[0];
        auto t20 = t[2] - t[0];

        auto u10 = t10.x;
        auto u20 = t20.x;
        auto v10 = t10.y;
        auto v20 = t20.y;

        // necessary?
        float dir = (u20 * v10 - u10 * v20) < 0 ? -1 : 1;

        // sanity check
        if (u20 * v10 == u10 * v20)
        {
            // glow::warning() << "Warning: Creating bad tangent";
            u20 = 1;
            u10 = 0;
            v10 = 1;
            v20 = 0;
        }
        auto tangent = dir * (p20 * v10 - p10 * v20);
        faceTangents[f] = tangent;
    }

    // compute halfedge tangents / interpolated vertex tangents
    for (auto f : faces())
    {
        for (auto h : f.halfedges())
        {
            auto halfedge_tangent = faceTangents[f] - normal[h] * dot(faceTangents[f], normal[h]);
            tangent[h] += halfedge_tangent;

            if (interpolate_tangents)
            {
                auto v = h.vertex_to();
                interpolated_tangents[v] += halfedge_tangent;
            }
        }
    }

    for (auto h : halfedges())
    {
        auto t = interpolate_tangents ? interpolated_tangents[h.vertex_to()] : tangent[h];
        tangent[h] = tg::normalize_safe(t);
    }

    return true;
}

glow::SharedVertexArray Mesh3D::createVertexArray() const
{
    std::vector<tg::pos3> aPos;
    std::vector<tg::vec3> aNormal;
    std::vector<tg::vec3> aTangent;
    std::vector<tg::pos2> aTexCoord;

    for (auto f : faces())
        for (auto h : f.halfedges())
        {
            auto v = h.vertex_to();
            aPos.push_back(position[v]);
            aNormal.push_back(normal[h]);
            aTangent.push_back(tangent[h]);
            aTexCoord.push_back(texCoord[h]);
        }

    auto abPos = glow::ArrayBuffer::create("aPosition", aPos);
    auto abNormal = glow::ArrayBuffer::create("aNormal", aNormal);
    auto abTangent = glow::ArrayBuffer::create("aTangent", aTangent);
    auto abTexCoord = glow::ArrayBuffer::create("aTexCoord", aTexCoord);

    // add index buffer if not a triangle mesh
    glow::SharedElementArrayBuffer eab;
    if (!is_triangle_mesh(*this))
    {
        std::vector<int> indices;
        indices.reserve(faces().size() * 3);

        auto v_base = 0;
        for (auto f : faces())
        {
            auto idx = 0;
            for (auto h : f.halfedges())
            {
                (void)h; // unused

                if (idx >= 2)
                {
                    indices.push_back(v_base + 0);
                    indices.push_back(v_base + idx - 1);
                    indices.push_back(v_base + idx - 0);
                }

                ++idx;
            }

            v_base += idx;
        }
        eab = glow::ElementArrayBuffer::create(indices);
    }

    return glow::VertexArray::create({abPos, abNormal, abTangent, abTexCoord}, eab, GL_TRIANGLES);
}
