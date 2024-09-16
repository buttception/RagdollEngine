#include "ragdollpch.h"

#include "GeometryBuilder.h"

void GeometryBuilder::Init(nvrhi::DeviceHandle nvrhiDevice)
{
	Device = nvrhiDevice;
	CommandList = nvrhiDevice->createCommandList();
}

// Helper for flipping winding of geometric primitives for LH vs. RH coords
void ReverseWinding(std::vector<uint32_t>& indices, std::vector<Vertex>& vertices)
{
    assert((indices.size() % 3) == 0);
    for (auto it = indices.begin(); it != indices.end(); it += 3)
    {
        std::swap(*it, *(it + 2));
    }

    for (auto& it : vertices)
    {
        it.texcoord.x = (1.f - it.texcoord.x);
    }
}

int32_t GeometryBuilder::BuildCube(float size)
{
    Vertices.clear();
    Vertices.clear();
	constexpr uint32_t faceCount = 6;
	static const Vector3 faceNormals[faceCount] = {
		{0.f, 0.f, 1.f},
		{0.f,  0.f, -1.f},
		{1.f,  0.f, 0.f},
		{-1.f,  0.f, 0.f},
		{0.f,  1.f, 0.f},
		{0.f, -1.f, 0.f},
	};
	static const Vector2 texcoords[4] = {
		{1, 0},
		{1, 1},
		{0, 1},
		{0, 0},
	};
	float halfExtents = size / 2.f;
    // Create each face in turn.
    for (int i = 0; i < faceCount; i++)
    {
        Vector3 normal = faceNormals[i];

        // Get two vectors perpendicular both to the face normal and to each other.
        Vector3 basis = (i >= 4) ? Vector3{ 0.f, 0.f, 1.f } : Vector3{ 0.f, 1.f, 0.f };

        Vector3 side1 = normal.Cross(basis);
        Vector3 side2 = normal.Cross(side1);

        // Six indices (two triangles) per face.
        const size_t vbase = Vertices.size();
        Indices.push_back(vbase + 0);
        Indices.push_back(vbase + 1);
        Indices.push_back(vbase + 2);

        Indices.push_back(vbase + 0);
        Indices.push_back(vbase + 2);
        Indices.push_back(vbase + 3);

        // Four vertices per face.
        // position // color // normal // tangent // binormal // t0
        // (normal - side1 - side2) * tsize
        Vertices.push_back({ (normal - side1 - side2) * halfExtents, {1.f, 1.f, 1.f, 1.f}, normal, Vector3::Zero, Vector3::Zero, texcoords[0] });

        // (normal - side1 + side2) * tsize
        Vertices.push_back({ (normal - side1 + side2) * halfExtents, {1.f, 1.f, 1.f, 1.f}, normal, Vector3::Zero, Vector3::Zero, texcoords[1] });

        // (normal + side1 + side2) * tsize
        Vertices.push_back({ (normal + side1 + side2) * halfExtents, {1.f, 1.f, 1.f, 1.f}, normal, Vector3::Zero, Vector3::Zero, texcoords[2] });

        // (normal + side1 - side2) * tsize
        Vertices.push_back({ (normal + side1 - side2) * halfExtents, {1.f, 1.f, 1.f, 1.f}, normal, Vector3::Zero, Vector3::Zero, texcoords[3] });
    }
    ReverseWinding(Indices, Vertices);

    uint32_t index = AssetManager::GetInstance()->AddVertices(Vertices, Indices);
    return index;
}

int32_t GeometryBuilder::BuildSphere(float diameter, uint32_t tessellation)
{
	Vertices.clear();
	Indices.clear();

	RD_ASSERT(tessellation < 3, "Invalid tessellation for sphere creation");

    const size_t verticalSegments = tessellation;
    const size_t horizontalSegments = tessellation * 2;

    const float radius = diameter / 2;

    // Create rings of vertices at progressively higher latitudes.
    for (size_t i = 0; i <= verticalSegments; i++)
    {
        const float v = 1 - float(i) / float(verticalSegments);

        const float latitude = (float(i) * DirectX::XM_PI / float(verticalSegments)) - DirectX::XM_PIDIV2;
        float dy, dxz;

        DirectX::XMScalarSinCos(&dy, &dxz, latitude);

        // Create a single ring of vertices at this latitude.
        for (size_t j = 0; j <= horizontalSegments; j++)
        {
            const float u = float(j) / float(horizontalSegments);

            const float longitude = float(j) * DirectX::XM_2PI / float(horizontalSegments);
            float dx, dz;

            DirectX::XMScalarSinCos(&dx, &dz, longitude);

            dx *= dxz;
            dz *= dxz;

            const Vector3 normal{ dx, dy, dz };
            const Vector2 texCoord{ u, v };
            const DirectX::XMVECTOR pos = DirectX::XMVectorScale(normal, radius);
            Vertices.push_back({ normal * radius, { 1.f,1.f,1.f,1.f }, normal, Vector3::Zero, Vector3::Zero, texCoord});
        }
    }

    // Fill the index buffer with triangles joining each pair of latitude rings.
    const size_t stride = horizontalSegments + 1;

    for (size_t i = 0; i < verticalSegments; i++)
    {
        for (size_t j = 0; j <= horizontalSegments; j++)
        {
            const size_t nextI = i + 1;
            const size_t nextJ = (j + 1) % stride;

            Indices.push_back(i * stride + j);
            Indices.push_back(nextI * stride + j);
            Indices.push_back(i * stride + nextJ);

            Indices.push_back(i * stride + nextJ);
            Indices.push_back(nextI * stride + j);
            Indices.push_back(nextI * stride + nextJ);
        }
    }
    ReverseWinding(Indices, Vertices);

    uint32_t index = AssetManager::GetInstance()->AddVertices(Vertices, Indices);
    return index;
}

Vector3 GetCircleVector(size_t i, size_t tessellation) 
{
    const float angle = float(i) * DirectX::XM_2PI / float(tessellation);
    float dx, dz;

    DirectX::XMScalarSinCos(&dx, &dz, angle);

    Vector3 v{ dx, 0.f, dz };
    return v;
}

DirectX::XMVECTOR GetCircleTangent(size_t i, size_t tessellation) noexcept
{
    const float angle = (float(i) * DirectX::XM_2PI / float(tessellation)) + DirectX::XM_PIDIV2;
    float dx, dz;

    DirectX::XMScalarSinCos(&dx, &dz, angle);

    const DirectX::XMVECTORF32 v = { { { dx, 0, dz, 0 } } };
    return v;
}

void CreateCylinderCap(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, size_t tessellation, float height, float radius, bool isTop)
{
    // Create cap indices.
    for (size_t i = 0; i < tessellation - 2; i++)
    {
        size_t i1 = (i + 1) % tessellation;
        size_t i2 = (i + 2) % tessellation;

        if (isTop)
        {
            std::swap(i1, i2);
        }

        const size_t vbase = vertices.size();
        indices.push_back(vbase);
        indices.push_back(vbase + i1);
        indices.push_back(vbase + i2);
    }

    // Which end of the cylinder is this?
    DirectX::XMVECTOR normal = DirectX::g_XMIdentityR1;
    DirectX::XMVECTOR textureScale = DirectX::g_XMNegativeOneHalf;

    if (!isTop)
    {
        normal = DirectX::XMVectorNegate(normal);
        textureScale = DirectX::XMVectorMultiply(textureScale, DirectX::g_XMNegateX);
    }

    // Create cap vertices.
    for (size_t i = 0; i < tessellation; i++)
    {
        const DirectX::XMVECTOR circleVector = GetCircleVector(i, tessellation);

        const DirectX::XMVECTOR position = DirectX::XMVectorAdd(DirectX::XMVectorScale(circleVector, radius), DirectX::XMVectorScale(normal, height));

        const DirectX::XMVECTOR textureCoordinate = DirectX::XMVectorMultiplyAdd(DirectX::XMVectorSwizzle<0, 2, 3, 3>(circleVector), textureScale, DirectX::g_XMOneHalf);

        Vector3 pos = position;
        Vector2 texCoord = textureCoordinate;
        Vector3 norm = normal;
        vertices.push_back({ pos, {1.f,1.f,1.f,1.f}, normal, Vector3::Zero, Vector3::Zero, texCoord});
    }
}

int32_t GeometryBuilder::BuildCylinder(float height, float diameter, size_t tessellation)
{
    Vertices.clear();
    Indices.clear();

    if (tessellation < 3)
        throw std::invalid_argument("tesselation parameter must be at least 3");

    height /= 2;

    const Vector3 topOffset{ 0.f, height, 0.f};

    const float radius = diameter / 2;
    const size_t stride = tessellation + 1;

    // Create a ring of triangles around the outside of the cylinder.
    for (size_t i = 0; i <= tessellation; i++)
    {
        const Vector3 normal = GetCircleVector(i, tessellation);

        const Vector3 sideOffset = normal * radius;

        const float u = float(i) / float(tessellation);

        Vector2 TexCoord{ u, 0.f };

        Vertices.push_back({ sideOffset + topOffset, { 1.f,1.f,1.f,1.f }, normal, Vector3::Zero, Vector3::Zero, TexCoord });
        Vertices.push_back({ sideOffset - topOffset, { 1.f,1.f,1.f,1.f }, normal, Vector3::Zero, Vector3::Zero, TexCoord });

        Indices.push_back(i * 2);
        Indices.push_back((i * 2 + 2) % (stride * 2));
        Indices.push_back(i * 2 + 1);

        Indices.push_back(i * 2 + 1);
        Indices.push_back((i * 2 + 2) % (stride * 2));
        Indices.push_back((i * 2 + 3) % (stride * 2));
    }

    // Create flat triangle fan caps to seal the top and bottom.
    CreateCylinderCap(Vertices, Indices, tessellation, height, radius, true);
    CreateCylinderCap(Vertices, Indices, tessellation, height, radius, false);

    ReverseWinding(Indices, Vertices);

    uint32_t index = AssetManager::GetInstance()->AddVertices(Vertices, Indices);
    return index;
}

int32_t GeometryBuilder::BuildCone(float diameter, float height, size_t tessellation)
{
    Vertices.clear();
    Indices.clear();

    if (tessellation < 3)
        throw std::invalid_argument("tesselation parameter must be at least 3");

    height /= 2;

    const DirectX::XMVECTOR topOffset = DirectX::XMVectorScale(DirectX::g_XMIdentityR1, height);

    const float radius = diameter / 2;
    const size_t stride = tessellation + 1;

    // Create a ring of triangles around the outside of the cone.
    for (size_t i = 0; i <= tessellation; i++)
    {
        const DirectX::XMVECTOR circlevec = GetCircleVector(i, tessellation);

        const DirectX::XMVECTOR sideOffset = DirectX::XMVectorScale(circlevec, radius);

        const float u = float(i) / float(tessellation);

        const DirectX::XMVECTOR textureCoordinate = DirectX::XMLoadFloat(&u);

        const DirectX::XMVECTOR pt = DirectX::XMVectorSubtract(sideOffset, topOffset);

        DirectX::XMVECTOR normal = DirectX::XMVector3Cross(
            GetCircleTangent(i, tessellation),
            DirectX::XMVectorSubtract(topOffset, pt));
        normal = DirectX::XMVector3Normalize(normal);

        // Duplicate the top vertex for distinct normals
        Vertices.push_back({ topOffset, Vector4::One, normal, Vector3::Zero, Vector3::Zero, Vector2::Zero });
        Vertices.push_back({ pt, Vector4::One, normal, Vector3::Zero, Vector3::Zero, DirectX::XMVectorAdd(textureCoordinate, DirectX::g_XMIdentityR1) });

        Indices.push_back(i * 2);
        Indices.push_back((i * 2 + 3) % (stride * 2));
        Indices.push_back((i * 2 + 1) % (stride * 2));
    }

    // Create flat triangle fan caps to seal the bottom.
    CreateCylinderCap(Vertices, Indices, tessellation, height, radius, false);

    ReverseWinding(Indices, Vertices);

    uint32_t index = AssetManager::GetInstance()->AddVertices(Vertices, Indices);
    return index;
}

int32_t GeometryBuilder::BuildIcosahedron(float size)
{
    Vertices.clear();
    Indices.clear();

    constexpr float  t = 1.618033988749894848205f; // (1 + sqrt(5)) / 2
    constexpr float t2 = 1.519544995837552493271f; // sqrt( 1 + sqr( (1 + sqrt(5)) / 2 ) )

    static const DirectX::XMVECTORF32 verts[12] =
    {
        { { {    t / t2,  1.f / t2,       0, 0 } } },
        { { {   -t / t2,  1.f / t2,       0, 0 } } },
        { { {    t / t2, -1.f / t2,       0, 0 } } },
        { { {   -t / t2, -1.f / t2,       0, 0 } } },
        { { {  1.f / t2,       0,    t / t2, 0 } } },
        { { {  1.f / t2,       0,   -t / t2, 0 } } },
        { { { -1.f / t2,       0,    t / t2, 0 } } },
        { { { -1.f / t2,       0,   -t / t2, 0 } } },
        { { {       0,    t / t2,  1.f / t2, 0 }  } },
        { { {       0,   -t / t2,  1.f / t2, 0 } } },
        { { {       0,    t / t2, -1.f / t2, 0 } } },
        { { {       0,   -t / t2, -1.f / t2, 0 } } }
    };

    static const uint32_t faces[20 * 3] =
    {
        0, 8, 4,
        0, 5, 10,
        2, 4, 9,
        2, 11, 5,
        1, 6, 8,
        1, 10, 7,
        3, 9, 6,
        3, 7, 11,
        0, 10, 8,
        1, 8, 10,
        2, 9, 11,
        3, 11, 9,
        4, 2, 0,
        5, 0, 2,
        6, 1, 3,
        7, 3, 1,
        8, 6, 4,
        9, 4, 6,
        10, 5, 7,
        11, 7, 5
    };

    for (size_t j = 0; j < std::size(faces); j += 3)
    {
        const uint32_t v0 = faces[j];
        const uint32_t v1 = faces[j + 1];
        const uint32_t v2 = faces[j + 2];

        DirectX::XMVECTOR normal = DirectX::XMVector3Cross(
            DirectX::XMVectorSubtract(verts[v1].v, verts[v0].v),
            DirectX::XMVectorSubtract(verts[v2].v, verts[v0].v));
        normal = DirectX::XMVector3Normalize(normal);

        const size_t base = Vertices.size();
        Indices.push_back(base);
        Indices.push_back(base + 1);
        Indices.push_back(base + 2);

        // Duplicate vertices to use face normals
        DirectX::XMVECTOR position = XMVectorScale(verts[v0], size);
        Vertices.push_back({ position, Vector4::One, normal, Vector3::Zero, Vector3::Zero, Vector2::Zero });

        position = XMVectorScale(verts[v1], size);
        Vertices.push_back({ position, Vector4::One, normal, Vector3::Zero, Vector3::Zero, {1.f, 0.f} });

        position = XMVectorScale(verts[v2], size);
        Vertices.push_back({ position, Vector4::One, normal, Vector3::Zero, Vector3::Zero, {0.f, 1.f} });
    }

    uint32_t index = AssetManager::GetInstance()->AddVertices(Vertices, Indices);
    return index;
}
