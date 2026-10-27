//============================================================================
// Name        : FarForest.cpp
// Author      : Duarte Peixinho
// Description : See FarForest.h.
//============================================================================

#include <Pyros3D/Rendering/Components/Foliage/FarForest.h>
#include <Pyros3D/Rendering/Components/Terrain/TerrainComponent.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingInstancedComponent.h>
#include <Pyros3D/Rendering/Renderer/InstanceCullGroup.h>
#include <Pyros3D/Assets/Renderable/Terrains/Heightfield.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Sphere.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cone.h>
#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>
#include <Pyros3D/Assets/Texture/PaintableImage.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Materials/GenericShaderMaterials/ShaderLib.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Utils/Jobs/JobSystem.h>
#include <Pyros3D/Utils/Json/json.hpp>
#include <Pyros3D/Core/Logs/Log.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <map>

namespace p3d {

	using json = nlohmann::json;

	namespace {

		struct Kind
		{
			FoliageLayerSpec spec;
			bool cone = false;
			f32 radius = 3.f, height = 6.f;
			Vec4 colour = Vec4(0.25f, 0.33f, 0.18f, 1.f);
			PaintableImage density;          // the whole terrain's; empty: grows everywhere
			// its impostor (a picture of the near tree from each of a few sides, and from above,
			// in one atlas): drawn as crossed cards; none, a canopy
			struct View { f32 angle = 0.f, left = 0.f, right = 0.f, bottom = 0.f, top = 0.f; Vec4 uv; bool above = false; };
			std::string atlas;
			std::vector<View> views;
		};

		// One tree of the far forest, in the world (the forest's own object is at the origin).
		struct Tree { Matrix m; Vec4 tint; uint32 kind; uint32 block; uint32 order; };

		inline uint32 Mix(uint32 x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }

		// A tree's impostor: a card for each picture of it - upright ones crossed about its axis, each
		// turned to the side it was pictured from, and a flat one across its crown for the view from
		// above. Their normals lean up, so that the sun lights the cards alike whichever way they face.
		class Cards : public Primitive
		{
		public:
			explicit Cards(const std::vector<Kind::View> &views)
			{
				calculateTangentBitangent = false;
				PrimitiveGeometry* g = geometry;
				f32 crown = 0.f, foot = 0.f;
				for (const Kind::View &v : views) if (!v.above) { crown = std::max(crown, v.top); foot = std::min(foot, v.bottom); }
				for (const Kind::View &v : views)
				{
					const uint32 first = (uint32)g->tVertex.size();
					Vec3 p[4], n;
					if (!v.above)
					{
						const f32 a = v.angle * 0.017453293f;
						const Vec3 right(cosf(a), 0.f, sinf(a)), normal(-sinf(a), 0.f, cosf(a));
						p[0] = right * v.left + Vec3(0.f, v.top, 0.f); p[1] = right * v.right + Vec3(0.f, v.top, 0.f);
						p[2] = right * v.right + Vec3(0.f, v.bottom, 0.f); p[3] = right * v.left + Vec3(0.f, v.bottom, 0.f);
						n = (normal * 0.3f + Vec3(0.f, 1.f, 0.f)).normalize();
					}
					else
					{
						// (pictured with the crown's top towards the camera: the picture's up is the tree's -z)
						const f32 y = foot + (crown - foot) * 0.62f;
						p[0] = Vec3(v.left, y, -v.top); p[1] = Vec3(v.right, y, -v.top);
						p[2] = Vec3(v.right, y, -v.bottom); p[3] = Vec3(v.left, y, -v.bottom);
						n = Vec3(0.f, 1.f, 0.f);
					}
					// (the picture's top row is v.uv.y)
					const Vec2 uv[4] = { Vec2(v.uv.x, v.uv.y), Vec2(v.uv.z, v.uv.y), Vec2(v.uv.z, v.uv.w), Vec2(v.uv.x, v.uv.w) };
					for (int k = 0; k < 4; k++)
					{
						g->tVertex.push_back(p[k]); g->tNormal.push_back(n); g->tTexcoord.push_back(uv[k]);
						g->tColor.push_back(Vec4(1.f, 1.f, 1.f, 1.f));
					}
					g->index.push_back(first); g->index.push_back(first + 1); g->index.push_back(first + 2);
					g->index.push_back(first); g->index.push_back(first + 2); g->index.push_back(first + 3);
				}
				Build();
			}
		};

		// A tree's crown as it is seen from a kilometre: a broadleaf's a few lobes, a conifer's a
		// few tiers, unit sized (radius 1, from y -1 to 1), darker underneath and inside - the light
		// a crown keeps from itself, in its vertex colours (ShaderUsage::VertexColor).
		class Canopy : public Primitive
		{
		public:
			explicit Canopy(const bool conifer)
			{
				calculateTangentBitangent = false;
				PrimitiveGeometry* g = geometry;
				auto shade = [](const f32 y, const f32 inner) { const f32 t = std::max(0.f, std::min(1.f, (y + 1.f) * 0.5f)); return (0.38f + 0.62f * t * t * (3.f - 2.f * t)) * inner; };
				auto tri = [g](const uint32 a, const uint32 b, const uint32 c, const Vec3 &centre) {
					const Vec3 n = (g->tVertex[b] - g->tVertex[a]).cross(g->tVertex[c] - g->tVertex[a]);
					const Vec3 mid = (g->tVertex[a] + g->tVertex[b] + g->tVertex[c]) * (1.f / 3.f);
					if (n.dotProduct(mid - centre) >= 0.f) { g->index.push_back(a); g->index.push_back(b); g->index.push_back(c); }
					else { g->index.push_back(a); g->index.push_back(c); g->index.push_back(b); }
				};
				if (!conifer)
				{
					// three lobes: the crown, and two on its shoulders
					const Vec4 lobes[3] = { Vec4(0.f, 0.08f, 0.f, 0.82f), Vec4(0.42f, -0.18f, 0.22f, 0.58f), Vec4(-0.38f, -0.12f, -0.30f, 0.6f) };
					const uint32 W = 7, H = 5;
					for (int l = 0; l < 3; l++)
					{
						const Vec3 c(lobes[l].x, lobes[l].y, lobes[l].z);
						const f32 r = lobes[l].w;
						const uint32 first = (uint32)g->tVertex.size();
						for (uint32 j = 0; j <= H; j++)
							for (uint32 i = 0; i <= W; i++)
							{
								const f32 th = (f32)j / H * 3.14159265f, ph = (f32)i / W * 6.2831853f;
								// (a little unevenness, the same at the seam)
								const f32 bump = 1.f + 0.10f * sinf(ph * 3.f + l * 1.7f) * sinf(th * 2.f);
								const Vec3 n(sinf(th) * cosf(ph), cosf(th), sinf(th) * sinf(ph));
								const Vec3 p = c + n * (r * bump);
								g->tVertex.push_back(p); g->tNormal.push_back(n);
								g->tTexcoord.push_back(Vec2((f32)i / W, (f32)j / H));
								const f32 inner = 0.82f + 0.18f * std::max(0.f, n.dotProduct((c * -1.f).normalize() * -1.f + Vec3(0.f, 0.3f, 0.f)));
								const f32 v = shade(p.y, l == 0 ? 1.f : inner);
								g->tColor.push_back(Vec4(v, v, v, 1.f));
							}
						for (uint32 j = 0; j < H; j++)
							for (uint32 i = 0; i < W; i++)
							{
								const uint32 a0 = first + j * (W + 1) + i, a1 = a0 + 1, b0 = a0 + (W + 1), b1 = b0 + 1;
								if (j > 0) tri(a0, a1, b1, c);
								if (j < H - 1) tri(a0, b1, b0, c);
							}
					}
				}
				else
				{
					// three tiers, each a cone wider than the one over it
					const f32 tiers[3][3] = { { 1.0f, 0.15f, 0.95f }, { 0.25f, -0.35f, 0.75f }, { -0.30f, -1.0f, 1.0f } };   // top y, bottom y, radius
					const uint32 W = 8;
					for (int t = 0; t < 3; t++)
					{
						const f32 top = tiers[t][0], bottom = tiers[t][1], r = tiers[t][2];
						const Vec3 axis(0.f, (top + bottom) * 0.5f, 0.f);
						const uint32 apex = (uint32)g->tVertex.size();
						g->tVertex.push_back(Vec3(0.f, top, 0.f)); g->tNormal.push_back(Vec3(0.f, 1.f, 0.f));
						g->tTexcoord.push_back(Vec2(0.5f, 0.f));
						{ const f32 v = shade(top, 1.f); g->tColor.push_back(Vec4(v, v, v, 1.f)); }
						const f32 slope = r / std::max(0.01f, top - bottom);
						for (uint32 i = 0; i <= W; i++)
						{
							const f32 ph = (f32)i / W * 6.2831853f;
							const Vec3 p(cosf(ph) * r, bottom, sinf(ph) * r);
							g->tVertex.push_back(p); g->tNormal.push_back(Vec3(cosf(ph), slope, sinf(ph)).normalize());
							g->tTexcoord.push_back(Vec2((f32)i / W, 1.f));
							const f32 v = shade(bottom, 0.85f); g->tColor.push_back(Vec4(v, v, v, 1.f));
						}
						for (uint32 i = 0; i < W; i++) tri(apex, apex + 1 + i, apex + 2 + i, axis);
					}
				}
				Build();
			}
		};

		Vec2 Pair(const json &j, const char* key, const Vec2 &d)
		{
			if (!j.contains(key) || !j[key].is_array() || j[key].size() < 2) return d;
			return Vec2(j[key][0].get<f32>(), j[key][1].get<f32>());
		}
	}

	std::shared_ptr<FarForest> FarForest::Build(SceneGraph* scene, const std::string &configPath, const std::string &assetRoot, std::string &error)
	{
		const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
		json cfg;
		{
			std::ifstream in(configPath);
			if (!in) { error = "cannot read " + configPath; return std::shared_ptr<FarForest>(); }
			try { in >> cfg; } catch (const std::exception &e) { error = std::string("bad config: ") + e.what(); return std::shared_ptr<FarForest>(); }
		}
		// the terrain: by its object's name, else the first with an overview
		TerrainComponent* terrain = NULL;
		const std::string want = cfg.value("terrain", std::string());
		for (TerrainComponent* t : TerrainComponent::Instances())
		{
			if (!t->GetOwner() || (scene && t->GetOwner()->GetScene() != scene) || !t->HasOverview()) continue;
			if (want.empty() || t->GetOwner()->GetName() == want) { terrain = t; break; }
		}
		if (!terrain) { error = "no terrain with an overview" + (want.empty() ? std::string() : " named " + want); return std::shared_ptr<FarForest>(); }
		const TerrainComponent::Settings &ts = terrain->GetSettings();

		std::vector<Kind> kinds;
		for (const json &k : cfg.value("kinds", json::array()))
		{
			Kind kd;
			FoliageLayerSpec &s = kd.spec;
			s.name = k.value("name", std::string());
			s.density = k.value("density", s.density);
			s.blockSize = k.value("blockSize", s.blockSize);
			s.minScale = k.value("minScale", s.minScale); s.maxScale = k.value("maxScale", s.maxScale);
			s.maxSlopeDegrees = k.value("maxSlopeDegrees", s.maxSlopeDegrees);
			s.minHeight = k.value("minHeight", s.minHeight); s.maxHeight = k.value("maxHeight", s.maxHeight);
			s.sink = k.value("sink", s.sink);
			s.seed = k.value("seed", s.seed);
			kd.cone = k.value("shape", std::string("round")) == "cone";
			const Vec2 size = Pair(k, "size", Vec2(kd.radius, kd.height));
			kd.radius = size.x; kd.height = size.y;
			if (k.contains("color") && k["color"].is_array() && k["color"].size() >= 3)
				kd.colour = Vec4(k["color"][0].get<f32>(), k["color"][1].get<f32>(), k["color"][2].get<f32>(), 1.f);
			if (k.contains("impostor") && k["impostor"].is_object())
			{
				const json &im = k["impostor"];
				kd.atlas = im.value("texture", std::string());
				for (const json &v : im.value("views", json::array()))
				{
					Kind::View w;
					w.angle = v.value("angle", 0.f); w.above = v.value("above", false);
					w.left = v.value("left", 0.f); w.right = v.value("right", 0.f); w.bottom = v.value("bottom", 0.f); w.top = v.value("top", 0.f);
					if (v.contains("uv") && v["uv"].is_array() && v["uv"].size() >= 4)
						w.uv = Vec4(v["uv"][0].get<f32>(), v["uv"][1].get<f32>(), v["uv"][2].get<f32>(), v["uv"][3].get<f32>());
					kd.views.push_back(w);
				}
				if (kd.views.empty()) kd.atlas.clear();
			}
			const std::string map = k.value("densityMap", std::string());
			if (!map.empty() && !kd.density.Load(assetRoot + "/" + map, 1))
				echo("WARNING: FarForest - no density map " + map + " (" + s.name + " grows everywhere)");
			kinds.push_back(std::move(kd));
		}
		if (kinds.empty()) { error = "no kinds"; return std::shared_ptr<FarForest>(); }

		const f32 keep = std::max(0.02f, std::min(1.f, cfg.value("keep", 0.3f)));
		// (how much darker an impostor is drawn than its picture: the picture was made in light no shadow
		// falls in, and a tree's crown near to is half in its own shade)
		const f32 shade = cfg.value("shade", 1.f);
		const int32 blockTiles = std::max(1, cfg.value("blockTiles", 4));
		const Vec2 fadeIn = Pair(cfg, "fadeIn", Vec2(700.f, 850.f));
		const Vec2 fade = Pair(cfg, "fade", Vec2(6500.f, 8000.f));
		f32 thinFrom = 1500.f, thinTo = 5000.f, thinLeast = 0.35f;
		if (cfg.contains("thin") && cfg["thin"].is_array() && cfg["thin"].size() >= 3)
		{ thinFrom = cfg["thin"][0].get<f32>(); thinTo = cfg["thin"][1].get<f32>(); thinLeast = cfg["thin"][2].get<f32>(); }
		// grown as the near ones are where they keep this share: wider more than taller
		const f32 growW = powf(1.f / keep, 0.75f), growH = powf(1.f / keep, 0.25f);

		// Every tile's trees, from the ground of the overview - the tiles shared between the cores
		const int32 TX = ts.tilesX, TZ = ts.tilesZ;
		const int32 BX = (TX + blockTiles - 1) / blockTiles, BZ = (TZ + blockTiles - 1) / blockTiles;
		const uint32 n = 33;                                 // ground samples a side (8 m apart on a 256 m tile)
		const Vec3 base = terrain->GetOwner()->GetWorldPosition();
		std::vector<std::vector<Tree> > perTile((size_t)TX * TZ);
		JobSystem::Instance().ParallelFor((uint32)(TX * TZ), 4, [&](uint32 a, uint32 b) {
			for (uint32 ti = a; ti < b; ti++)
			{
				const int32 x = (int32)(ti % TX), z = (int32)(ti / TX);
				const Vec3 origin = terrain->TileOrigin(x, z);
				// the tile's own ground, as its near trees grow on it (from the overview, four metres a sample,
				// trees by a river or a lake stood in the air between the bank and the water)
				HeightfieldData g;
				const std::string root = terrain->GetAssetRoot().empty() ? assetRoot : terrain->GetAssetRoot();
				if (!HeightfieldData::LoadFile(root + "/" + terrain->HeightmapPath(x, z), ts.tileSize, terrain->HeightScale(), terrain->HeightOffset(), g))
				{
					g = HeightfieldData();
					g.samples = n; g.size = ts.tileSize;
					g.heights.resize((size_t)n * n);
					const f32 step = ts.tileSize / (f32)(n - 1);
					g.minHeight = 1e30f; g.maxHeight = -1e30f;
					for (uint32 r = 0; r < n; r++)
						for (uint32 c = 0; c < n; c++)
						{
							f32 h = 0.f;
							terrain->OverviewHeightAt(origin.x + c * step, origin.z + r * step, h);
							h -= base.y;
							g.heights[(size_t)r * n + c] = h;
							g.minHeight = std::min(g.minHeight, h); g.maxHeight = std::max(g.maxHeight, h);
						}
					g.rangeMin = g.minHeight; g.rangeMax = g.maxHeight;
				}
				std::vector<Tree> &out = perTile[ti];
				const int32 bx = x / blockTiles, bz = z / blockTiles;
				for (uint32 k = 0; k < kinds.size(); k++)
				{
					const Kind &kd = kinds[k];
					// this tile's own numbers, as the terrain gives its near layer (BuildTileJson)
					FoliageLayerSpec s = kd.spec;
					s.seed = kd.spec.seed + (uint32)(z * TX + x) * 7919u;
					// the tile's own density map, as its near layer reads it (BuildTileJson); none: everywhere
					PaintableImage own;
					const PaintableImage* map = NULL;
					const std::string root2 = terrain->GetAssetRoot().empty() ? assetRoot : terrain->GetAssetRoot();
					if (own.Load(root2 + "/" + ts.directory + "/" + TerrainComponent::TileStem(x, z) + "_" + kd.spec.name + "_density.png", 1)) map = &own;
					PreparedFoliageLayer prepared;
					PreparedFoliageLayer::Generate(g, s, map, prepared);
					for (const FoliageBlock &fb : prepared.blocks)
					{
						const size_t count = fb.transforms.size();
						for (size_t i = 0; i < count; i++)
						{
							// (the near layer keeps the first of each block's plants where it is thinnest)
							if (((f32)i + 0.5f) / (f32)count >= keep) break;
							Matrix m = fb.transforms[i];
							const Vec3 at = origin + fb.origin + Vec3(m.m[12], m.m[13], m.m[14]);
							m.m[12] = at.x; m.m[13] = at.y - base.y; m.m[14] = at.z;
							// the canopy: a unit sphere (or cone) raised to the crown and sized to it
							Matrix c;
							c.identity();
							c.m[0] = kd.radius * growW; c.m[5] = kd.height * 0.5f * growH; c.m[10] = kd.radius * growW;
							c.m[13] = kd.height * (kd.cone ? 0.62f : 0.68f) * growH;
							Tree t;
							const bool cards = !kd.atlas.empty();
							if (cards)
							{
								// (the cards are the tree at scale 1: grown as the near ones are)
								Matrix gm; gm.identity(); gm.m[0] = growW; gm.m[5] = growH; gm.m[10] = growW;
								t.m = m * gm;
							}
							else t.m = m * c;
							const uint32 h = Mix((uint32)(at.x * 7.f) * 73856093u ^ (uint32)(at.z * 7.f) * 19349663u ^ k);
							const f32 v = 0.82f + 0.36f * (f32)(h & 1023) / 1023.f;
							t.tint = cards ? Vec4(v * shade, v * shade, v * shade, 1.f) : Vec4(kd.colour.x * v, kd.colour.y * v, kd.colour.z * v, 1.f);
							t.kind = k;
							t.block = (uint32)(bz * BX + bx);
							t.order = h;
							out.push_back(t);
						}
					}
				}
			}
		});

		// Into blocks (blockTiles a side), a shape each - shuffled, so that thinning by a plant's
		// place in its block (the kernel's) takes evenly from every kind and every tile
		std::shared_ptr<FarForest> ff(new FarForest());
		ff->scene = scene;
		ff->owner = std::make_shared<GameObject>();
		ff->owner->SetName("FarForest");
		ff->owner->SetTransient(true);
		ff->owner->SetPosition(base);
		const size_t K = kinds.size();
		std::vector<std::vector<Tree> > lists((size_t)BX * BZ * K);
		for (std::vector<Tree> &tile : perTile)
			for (const Tree &t : tile) lists[(size_t)t.block * K + t.kind].push_back(t);
		perTile.clear();

		std::shared_ptr<Renderable> canopies[2];
		for (size_t k = 0; k < K; k++)
		{
			const Kind &kd = kinds[k];
			std::shared_ptr<Renderable> mesh;
			std::shared_ptr<GenericShaderMaterial> mat;
			f32 meshRadius = 1.f;
			if (!kd.atlas.empty())
			{
				mesh = std::make_shared<Cards>(kd.views);
				mat = std::make_shared<GenericShaderMaterial>((uint32)(ShaderUsage::Texture | ShaderUsage::Diffuse | ShaderUsage::AlphaTest
					| ShaderUsage::InstancedRendering | ShaderUsage::InstancedColor));
				if (std::shared_ptr<Texture> tex = Texture::LoadShared(assetRoot + "/" + kd.atlas, TextureType::Texture, true, true)) mat->SetColorMap(tex);
				mat->SetAlphaCutoff(0.5f);
				mat->SetCullFace(CullFace::DoubleSided);
				for (const Kind::View &v : kd.views) meshRadius = std::max(meshRadius, std::max(std::max(fabsf(v.left), fabsf(v.right)), std::max(fabsf(v.top), fabsf(v.bottom))));
			}
			else
			{
				const int sh = kd.cone ? 1 : 0;
				if (!canopies[sh]) canopies[sh] = std::make_shared<Canopy>(kd.cone);
				mesh = canopies[sh];
				mat = std::make_shared<GenericShaderMaterial>((uint32)(ShaderUsage::Color | ShaderUsage::Diffuse | ShaderUsage::VertexColor
					| ShaderUsage::InstancedRendering | ShaderUsage::InstancedColor));
				meshRadius = 1.f + (kd.cone ? 1.f : 0.f);
			}
			mat->SetColor(Vec4(1.f, 1.f, 1.f, 1.f));
			mat->SetRoughness(0.95f);
			std::shared_ptr<InstanceCullGroup> group;
			if (InstanceCullGroup::Available())
			{
				group = std::make_shared<InstanceCullGroup>(ff->owner.get(), mesh, std::static_pointer_cast<IMaterial>(mat),
					std::shared_ptr<Renderable>(), 0.f, true, meshRadius);
				group->SetThinning(thinFrom, thinTo, thinLeast, fade.x, fade.y);
				group->SetFadeIn(fadeIn.x, fadeIn.y);
				ff->groups.push_back(group);
			}
			for (int32 b = 0; b < BX * BZ; b++)
			{
				std::vector<Tree> &L = lists[(size_t)b * K + k];
				if (L.empty()) continue;
				std::sort(L.begin(), L.end(), [](const Tree &a, const Tree &c) { return a.order < c.order; });
				Vec3 lo(1e30f, 1e30f, 1e30f), hi(-1e30f, -1e30f, -1e30f);
				for (const Tree &t : L)
				{
					const Vec3 p(t.m.m[12], t.m.m[13], t.m.m[14]);
					lo = Vec3(std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z));
					hi = Vec3(std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z));
				}
				const Vec3 mid = (lo + hi) * 0.5f;
				std::vector<Matrix> ms; std::vector<Vec4> tints;
				ms.reserve(L.size()); tints.reserve(L.size());
				for (const Tree &t : L)
				{
					Matrix m = t.m;
					m.m[12] -= mid.x; m.m[13] -= mid.y; m.m[14] -= mid.z;
					ms.push_back(m); tints.push_back(t.tint);
				}
				ff->trees += (uint32)L.size();
				const f32 radius = (hi - lo).magnitude() * 0.5f + 40.f;
				std::shared_ptr<GameObject> go = std::make_shared<GameObject>();
				go->SetName("FarForest_" + std::to_string(b) + "_" + kd.spec.name);
				go->SetTransient(true);
				go->SetPosition(mid);
				std::shared_ptr<RenderingInstancedComponent> ic = std::make_shared<RenderingInstancedComponent>(mesh,
					std::static_pointer_cast<IMaterial>(mat), std::move(ms), std::move(tints), radius);
				ic->DisableCastShadows();
				go->AddComponent(ic);
				if (group) group->Add(ic.get());
				ff->owner->Add(go);
				ff->blocks.push_back(go);
				std::vector<Tree>().swap(L);
			}
		}
		if (scene) scene->Add(ff->owner);
		const f64 ms = std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - t0).count();
		echo("FarForest: " + std::to_string(ff->trees) + " trees in " + std::to_string(ff->blocks.size()) + " blocks, " + std::to_string((int)ms) + " ms");
		return ff;
	}

	FarForest::~FarForest()
	{
		for (size_t i = 0; i < groups.size(); i++) if (groups[i]) groups[i]->Detach();
		if (scene && owner) scene->Remove(owner);
	}

}
