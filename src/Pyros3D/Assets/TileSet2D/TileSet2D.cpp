//============================================================================
// Name        : TileSet2D.cpp
// Description : Reading and writing .p3dt - see the header for what one is.
//
//               JSON for the same reason a .p3d2d is: a tileset is small, and
//               it is exactly the kind of file a person wants to diff or fix
//               by hand after a bad re-slice.
//============================================================================

#include <cmath>
#include <Pyros3D/Assets/TileSet2D/TileSet2D.h>
#include <Pyros3D/Utils/Json/json.hpp>
#include <Pyros3D/Ext/stb/stb_image.h>
#include <fstream>
#include <sstream>

namespace p3d {

	namespace {

		using json = nlohmann::json;

		const int32 kTileSet2DVersion = 1;

		void Fail(std::string* errorOut, const std::string &msg)
		{
			if (errorOut) *errorOut = msg;
		}

		// How many whole cells fit along an axis. n cells occupy
		// margin + n*size + (n-1)*spacing + margin, so solving for n gives
		// this. The +spacing in the numerator is that (n-1) - drop it and a
		// sheet whose last cell ends flush against the margin loses that
		// cell, which looks like an off-by-one in the art rather than here.
		int32 FitCount(const int32 extent, const int32 size,
			const int32 margin, const int32 spacing)
		{
			if (size < 1) return 0;
			const int32 usable = extent - 2 * margin + spacing;
			if (usable < 1) return 0;
			const int32 n = usable / (size + spacing);
			return n > 0 ? n : 0;
		}

	}

	void TileSet2D::SetImageSize(const int32 w, const int32 h)
	{
		imageW = w > 0 ? w : 0;
		imageH = h > 0 ? h : 0;
	}

	int32 TileSet2D::Columns() const
	{
		if (columns > 0) return columns;
		if (!HaveImageSize()) return 0;
		return FitCount(imageW, tileW, margin, spacing);
	}

	int32 TileSet2D::Rows() const
	{
		if (!HaveImageSize()) return 0;
		return FitCount(imageH, tileH, margin, spacing);
	}

	int32 TileSet2D::TileCount() const
	{
		// Columns() can be the authored override, which may be narrower than
		// the sheet - so the count is the grid actually addressed, not what
		// the image could hold.
		return Columns() * Rows();
	}

	Vec4 TileSet2D::UVRect(const int32 index) const
	{
		const int32 cols = Columns();
		if (cols < 1 || index < 0 || index >= TileCount() || !HaveImageSize())
			return Vec4(0.f, 0.f, 0.f, 0.f);

		const int32 col = index % cols;
		const int32 row = index / cols;

		const f32 px = (f32)(margin + col * (tileW + spacing));
		const f32 py = (f32)(margin + row * (tileH + spacing));

		const f32 w = (f32)imageW;
		const f32 h = (f32)imageH;

		// Half a texel in from each edge - see the header.
		return Vec4(
			(px + 0.5f) / w,
			(py + 0.5f) / h,
			(px + (f32)tileW - 0.5f) / w,
			(py + (f32)tileH - 0.5f) / h);
	}

	const TileInfo2D* TileSet2D::Find(const int32 index) const
	{
		std::map<int32, TileInfo2D>::const_iterator it = tiles.find(index);
		return it == tiles.end() ? NULL : &it->second;
	}

	bool TileSet2D::IsSolid(const int32 index) const
	{
		const TileInfo2D* t = Find(index);
		return t != NULL && t->solid;
	}

	f32 TileShape2DHeight(const int32 shape, const f32 t)
	{
		const f32 x = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
		switch (shape)
		{
			case TileShape2D::SlopeBR: return x;
			case TileShape2D::SlopeBL: return 1.f - x;
			// Quarter circle centred on the cell corner the surface rises
			// TOWARDS, so the profile leaves that corner vertically and the
			// opposite one horizontally - a crest.
			case TileShape2D::ArcConvexBR:
				return std::sqrt(1.f - (1.f - x) * (1.f - x));
			case TileShape2D::ArcConvexBL:
				return std::sqrt(1.f - x * x);
			// The complement: centred on the corner the surface rises FROM,
			// so it starts flat and turns up hard - a valley wall.
			case TileShape2D::ArcConcaveBR:
				return 1.f - std::sqrt(1.f - x * x);
			case TileShape2D::ArcConcaveBL:
				return 1.f - std::sqrt(1.f - (1.f - x) * (1.f - x));
			default: return 1.f;
		}
	}

	bool TileShape2DIsCeilProfile(const int32 shape)
	{
		return shape == TileShape2D::ArcCeilConvexBR
			|| shape == TileShape2D::ArcCeilConvexBL
			|| shape == TileShape2D::ArcCeilConcaveBR
			|| shape == TileShape2D::ArcCeilConcaveBL;
	}

	int32 TileShape2DCeilBase(const int32 shape)
	{
		switch (shape)
		{
			case TileShape2D::ArcCeilConvexBR:  return TileShape2D::ArcConvexBR;
			case TileShape2D::ArcCeilConvexBL:  return TileShape2D::ArcConvexBL;
			case TileShape2D::ArcCeilConcaveBR: return TileShape2D::ArcConcaveBR;
			case TileShape2D::ArcCeilConcaveBL: return TileShape2D::ArcConcaveBL;
			default: return TileShape2D::Box;
		}
	}

	bool TileShape2DIsFloorProfile(const int32 shape)
	{
		switch (shape)
		{
			case TileShape2D::SlopeBR:
			case TileShape2D::SlopeBL:
			case TileShape2D::ArcConvexBR:
			case TileShape2D::ArcConvexBL:
			case TileShape2D::ArcConcaveBR:
			case TileShape2D::ArcConcaveBL:
				return true;
			default: return false;
		}
	}

	const char* TileShape2DName(const int32 shape)
	{
		switch (shape)
		{
			case TileShape2D::ArcCeilConvexBR:  return "arc_ceil_convex_br";
			case TileShape2D::ArcCeilConvexBL:  return "arc_ceil_convex_bl";
			case TileShape2D::ArcCeilConcaveBR: return "arc_ceil_concave_br";
			case TileShape2D::ArcCeilConcaveBL: return "arc_ceil_concave_bl";
			case TileShape2D::ArcConvexBR:  return "arc_convex_br";
			case TileShape2D::ArcConvexBL:  return "arc_convex_bl";
			case TileShape2D::ArcConcaveBR: return "arc_concave_br";
			case TileShape2D::ArcConcaveBL: return "arc_concave_bl";
			case TileShape2D::SlopeBR: return "slope_br";
			case TileShape2D::SlopeBL: return "slope_bl";
			case TileShape2D::SlopeTR: return "slope_tr";
			case TileShape2D::SlopeTL: return "slope_tl";
			default: return "box";
		}
	}

	int32 TileShape2DFromName(const std::string &name)
	{
		if (name == "arc_ceil_convex_br")  return TileShape2D::ArcCeilConvexBR;
		if (name == "arc_ceil_convex_bl")  return TileShape2D::ArcCeilConvexBL;
		if (name == "arc_ceil_concave_br") return TileShape2D::ArcCeilConcaveBR;
		if (name == "arc_ceil_concave_bl") return TileShape2D::ArcCeilConcaveBL;
		if (name == "arc_convex_br")  return TileShape2D::ArcConvexBR;
		if (name == "arc_convex_bl")  return TileShape2D::ArcConvexBL;
		if (name == "arc_concave_br") return TileShape2D::ArcConcaveBR;
		if (name == "arc_concave_bl") return TileShape2D::ArcConcaveBL;
		if (name == "slope_br") return TileShape2D::SlopeBR;
		if (name == "slope_bl") return TileShape2D::SlopeBL;
		if (name == "slope_tr") return TileShape2D::SlopeTR;
		if (name == "slope_tl") return TileShape2D::SlopeTL;
		return TileShape2D::Box;
	}

	int32 TileSet2D::Shape(const int32 index) const
	{
		const TileInfo2D* t = Find(index);
		return t == NULL ? TileShape2D::Box : t->shape;
	}

	bool TileSet2D::IsSloped(const int32 index) const
	{
		const TileInfo2D* t = Find(index);
		return t != NULL && t->solid && t->shape != TileShape2D::Box;
	}

	int32 TileSet2D::AutoTileForTile(const int32 index) const
	{
		for (size_t i = 0; i < autotiles.size(); i++)
			if (index >= autotiles[i].base
				&& index < autotiles[i].base + kAutoTileCount) return (int32)i;
		return -1;
	}

	int32 TileSet2D::AutoTileAt(const int32 group, const int32 mask) const
	{
		if (group < 0 || (size_t)group >= autotiles.size()) return -1;
		const int32 m = mask < 0 ? 0 : (mask > 15 ? 15 : mask);
		return autotiles[(size_t)group].base + m;
	}

	int32 TileSet2D::AnimForTile(const int32 index) const
	{
		for (size_t i = 0; i < anims.size(); i++)
			if (!anims[i].frames.empty() && anims[i].frames[0] == index) return (int32)i;
		return -1;
	}

	int32 TileSet2D::AnimFrameAt(const int32 anim, const f32 seconds) const
	{
		if (anim < 0 || (size_t)anim >= anims.size()) return -1;
		const TileAnim2D &a = anims[(size_t)anim];
		if (a.frames.empty()) return -1;
		if (a.frames.size() == 1 || a.fps <= 0.f) return a.frames[0];
		const f64 adv = (f64)seconds * (f64)a.fps;
		// fmod on the frame COUNT, not the duration: a long-running level
		// would lose precision doing it in seconds, and the result here only
		// ever needs to be an index.
		int64 k = (int64)adv % (int64)a.frames.size();
		if (k < 0) k += (int64)a.frames.size();
		return a.frames[(size_t)k];
	}

	bool TileSet2D::HasTag(const int32 index, const std::string &tag) const
	{
		const TileInfo2D* t = Find(index);
		if (t == NULL) return false;
		for (size_t i = 0; i < t->tags.size(); i++)
			if (t->tags[i] == tag) return true;
		return false;
	}

	const std::vector<std::string> &TileSet2D::Tags(const int32 index) const
	{
		// A shared empty rather than a temporary: this returns a reference,
		// and the sparse map means most cells have no TileInfo2D at all.
		static const std::vector<std::string> none;
		const TileInfo2D* t = Find(index);
		return t == NULL ? none : t->tags;
	}

	bool TileSet2DReadImageSize(const std::string &resolvedPath, int32 &w, int32 &h)
	{
		int iw = 0, ih = 0, comp = 0;
		if (!stbi_info(resolvedPath.c_str(), &iw, &ih, &comp)) return false;
		w = (int32)iw;
		h = (int32)ih;
		return iw > 0 && ih > 0;
	}

	std::string TileSet2DToString(const TileSet2D &set)
	{
		json j;
		j["version"] = kTileSet2DVersion;
		j["image"] = set.image;
		j["tileW"] = set.tileW;
		j["tileH"] = set.tileH;
		// Omitted when zero: the common sheet has neither, and writing two
		// zeroes into every file buries the two knobs that do matter.
		if (set.margin != 0) j["margin"] = set.margin;
		if (set.spacing != 0) j["spacing"] = set.spacing;
		if (set.columns > 0) j["columns"] = set.columns;

		json arr = json::array();
		for (std::map<int32, TileInfo2D>::const_iterator it = set.tiles.begin();
			it != set.tiles.end(); ++it)
		{
			// A cell that says nothing is not worth a line.
			if (!it->second.solid && it->second.tags.empty()
				&& it->second.shape == TileShape2D::Box) continue;
			json t;
			t["i"] = it->first;
			if (it->second.solid) t["solid"] = true;
			if (!it->second.tags.empty()) t["tags"] = it->second.tags;
			// Omitted when it is a plain box, so a tileset that predates
			// shapes round-trips byte-identical.
			if (it->second.shape != TileShape2D::Box)
				t["shape"] = TileShape2DName(it->second.shape);
			arr.push_back(t);
		}
		if (!arr.empty()) j["tiles"] = arr;
		// Animations. Omitted entirely when there are none, so a static
		// tileset round-trips exactly as it did before they existed.
		if (!set.anims.empty())
		{
			nlohmann::json an = nlohmann::json::array();
			for (size_t i = 0; i < set.anims.size(); i++)
			{
				if (set.anims[i].frames.empty()) continue;
				nlohmann::json a1;
				if (!set.anims[i].name.empty()) a1["name"] = set.anims[i].name;
				a1["frames"] = set.anims[i].frames;
				a1["fps"] = set.anims[i].fps;
				an.push_back(a1);
			}
			if (!an.empty()) j["anims"] = an;
		}
		if (!set.autotiles.empty())
		{
			nlohmann::json at = nlohmann::json::array();
			for (size_t i = 0; i < set.autotiles.size(); i++)
			{
				nlohmann::json a1;
				if (!set.autotiles[i].name.empty()) a1["name"] = set.autotiles[i].name;
				a1["base"] = set.autotiles[i].base;
				at.push_back(a1);
			}
			j["autotiles"] = at;
		}

		return j.dump(1, '\t');
	}

	bool TileSet2DFromString(const std::string &text, TileSet2D &out,
		std::string *errorOut)
	{
		json j;
		try {
			j = json::parse(text);
		}
		catch (const std::exception &e) {
			Fail(errorOut, std::string("malformed tileset JSON: ") + e.what());
			return false;
		}
		if (!j.is_object()) { Fail(errorOut, "tileset is not a JSON object"); return false; }

		TileSet2D s;
		s.image = j.value("image", std::string());
		s.tileW = j.value("tileW", 16);
		s.tileH = j.value("tileH", 16);
		s.margin = j.value("margin", 0);
		s.spacing = j.value("spacing", 0);
		s.columns = j.value("columns", 0);

		// Checked rather than clamped. Every one of these silently cuts the
		// wrong cells instead of failing, and a tileset is authored once and
		// read forever - the report belongs at the point it can still be
		// fixed.
		if (s.tileW < 1 || s.tileH < 1)
		{
			Fail(errorOut, "tileW and tileH must be at least 1");
			return false;
		}
		if (s.margin < 0 || s.spacing < 0)
		{
			Fail(errorOut, "margin and spacing cannot be negative");
			return false;
		}
		if (s.columns < 0)
		{
			Fail(errorOut, "columns cannot be negative (0 means derive it)");
			return false;
		}

		if (j.contains("autotiles") && j["autotiles"].is_array())
			for (size_t i = 0; i < j["autotiles"].size(); i++)
			{
				const nlohmann::json &a1 = j["autotiles"][i];
				if (!a1.is_object() || !a1.contains("base")) continue;
				AutoTile2D at;
				at.name = a1.value("name", std::string());
				at.base = (int32)a1.value("base", 0);
				s.autotiles.push_back(at);
			}
		if (j.contains("anims") && j["anims"].is_array())
		{
			for (size_t i = 0; i < j["anims"].size(); i++)
			{
				const nlohmann::json &a1 = j["anims"][i];
				if (!a1.is_object() || !a1.contains("frames")
					|| !a1["frames"].is_array()) continue;
				TileAnim2D an;
				an.name = a1.value("name", std::string());
				an.fps = (f32)a1.value("fps", 8.0);
				for (size_t k = 0; k < a1["frames"].size(); k++)
					if (a1["frames"][k].is_number())
						an.frames.push_back((int32)a1["frames"][k].get<int>());
				if (!an.frames.empty()) s.anims.push_back(an);
			}
		}
		if (j.contains("tiles") && j["tiles"].is_array())
		{
			for (size_t i = 0; i < j["tiles"].size(); i++)
			{
				const json &t = j["tiles"][i];
				if (!t.is_object() || !t.contains("i")) continue;
				const int32 idx = t["i"].get<int32>();
				if (idx < 0) continue;
				TileInfo2D info;
				info.solid = t.value("solid", false);
				if (t.contains("shape") && t["shape"].is_string())
					info.shape = TileShape2DFromName(t["shape"].get<std::string>());
				if (t.contains("tags") && t["tags"].is_array())
					for (size_t k = 0; k < t["tags"].size(); k++)
						if (t["tags"][k].is_string())
							info.tags.push_back(t["tags"][k].get<std::string>());
				s.tiles[idx] = info;
			}
		}

		out = s;
		return true;
	}

	bool LoadTileSet2D(const std::string &filename, TileSet2D &out,
		std::string *errorOut)
	{
		std::ifstream in(filename.c_str());
		if (!in.is_open())
		{
			Fail(errorOut, "could not open " + filename);
			return false;
		}
		std::stringstream ss;
		ss << in.rdbuf();
		std::string err;
		if (!TileSet2DFromString(ss.str(), out, &err))
		{
			Fail(errorOut, filename + ": " + err);
			return false;
		}
		return true;
	}

	bool SaveTileSet2D(const std::string &filename, const TileSet2D &set,
		std::string *errorOut)
	{
		std::ofstream o(filename.c_str());
		if (!o.is_open())
		{
			Fail(errorOut, "could not write " + filename);
			return false;
		}
		o << TileSet2DToString(set);
		return true;
	}

}
