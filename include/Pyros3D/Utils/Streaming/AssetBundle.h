//============================================================================
// Name        : AssetBundle.h
// Author      : Duarte Peixinho
// Description : The CPU half of loading a set of assets, done ahead of time.
//
//               Loading anything is two jobs: reading and decoding (files,
//               PNGs, .p3dm parsing - pure CPU, any thread) and creating GPU
//               objects (main thread only, where the device lives). A bundle
//               holds the first job's results. Fill it on whatever thread
//               suits - a scene load spreads it over the job system, the
//               streamer does it on its own loader threads - then, on the
//               main thread, open a Use scope around the ordinary load code:
//               Texture::LoadTexture and the Model constructor take their
//               bytes from the bundle instead of the disk. Nothing else about
//               loading changes, which is the point.
//
//               Each image or model in a bundle is taken by one load. What no
//               load claimed is freed with the bundle.
//============================================================================

#ifndef ASSETBUNDLE_H
#define ASSETBUNDLE_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Assets/Renderable/Terrains/Heightfield.h>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace p3d {

	class ModelLoader;

	class PYROS3D_API AssetBundle
	{
	public:
		AssetBundle();
		~AssetBundle();
		AssetBundle(const AssetBundle &) = delete;
		AssetBundle &operator=(const AssetBundle &) = delete;

		// Thread-safe; each decodes or parses on the calling thread.
		// AddModel also reports the texture paths the model's materials
		// name, so the caller can add those. False when the file could not
		// be read - nothing is kept, and the real load reports the error.
		bool AddImage(const std::string &path);
		bool AddModel(const std::string &path, std::vector<std::string>* texturesOut = NULL);

		// Adds every model, then every image, including the textures the
		// models name. parallel spreads the work over the job system and
		// blocks until it is done - right for a load the caller waits on,
		// wrong on a background thread, where it would steal the workers a
		// physics step is about to wait for. Biggest files go first so one
		// large texture does not finish alone at the end.
		//
		// An image already alive in Texture::LoadShared's cache is skipped -
		// its load will not decode. modelTexturesOut, when given, receives
		// the texture paths the models' materials name.
		void Fill(const std::vector<std::string> &models, const std::vector<std::string> &images, bool parallel,
			std::vector<std::string>* modelTexturesOut = NULL);

		// A terrain tile, prepared by the caller (PreparedHeightfield::
		// Prepare, on any thread) and filed under its Key().
		void AddHeightfield(const std::string &key, const std::shared_ptr<PreparedHeightfield> &prepared);

		size_t ImageCount() const;
		size_t ModelCount() const;
		// Pixels plus parsed vertex data, roughly - for budgets and traces.
		size_t ByteSize() const;

		// While a Use is open on the main thread, loads take from this
		// bundle first. Uses nest; the innermost wins.
		class PYROS3D_API Use
		{
		public:
			explicit Use(AssetBundle &bundle);
			~Use();
		private:
			AssetBundle* previous;
		};

		// For Texture::LoadTexture and Model's constructor. NULL / false
		// when no bundle is open or it does not hold the path.
		static bool TakeImage(const std::string &path, Texture::DecodedImage &out);
		static ModelLoader* TakeModel(const std::string &path);
		static std::shared_ptr<PreparedHeightfield> TakeHeightfield(const std::string &key);

	private:
		mutable std::mutex mutex;
		std::map<std::string, Texture::DecodedImage> images;
		std::map<std::string, std::unique_ptr<ModelLoader> > models;
		std::map<std::string, std::shared_ptr<PreparedHeightfield> > heightfields;
		size_t bytes;
	};

}

#endif /* ASSETBUNDLE_H */
