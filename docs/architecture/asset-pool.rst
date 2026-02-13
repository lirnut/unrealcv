Asset Pool System Architecture
===========================

Overview
--------

The Asset Pool System (``FAssetPoolManager``) is a singleton class that manages asset metadata
for randomized scene generation. It maintains pools of assets organized by category, enabling
efficient random selection of foreground actors and occluders during dataset production.

**Header:** ``Source/UnrealCV/Public/Utils/AssetPoolManager.h``

Architecture
-----------

.. code-block::

   +-------------------+
   | FAssetPoolManager |  <-- Singleton instance
   +-------------------+
   | - AssetPools      |  <-- TMap<Category, Array<Metadata>>
   +-------------------+
   | + GetRandomAsset()|
   | + RegisterAsset() |
   | + GetAssetCount() |
   +--------+----------+
            |
            v
   +-------------------+
   |    AssetPools     |
   +-------------------+
   | Foreground_Human  | --> [{Path, Type, AnimSeq}, ...]
   | Foreground_Pet    | --> [{Path, Type, AnimSeq}, ...]
   | Occluder_All      | --> [{Path, Type, AnimSeq}, ...]
   | Scene_Indoor      | --> [{Path, Type, AnimSeq}, ...]
   +-------------------+

Asset Metadata Structure
----------------------

Each asset stores metadata as ``TMap<FString, FString>``:

.. code-block:: cpp

   TMap<FString, FString> Metadata = {
      {"Path", "/Game/Assets/Human_01/Human_01"},           // REQUIRED
      {"Type", "SM+AnimSeq"},                                 // REQUIRED
      {"AnimSequence", "/Game/Animations/Walk/Walk_Cycle"}   // Conditional
   };

**Required Fields:**

   - ``Path``: Main asset path (required for all types)
   - ``Type``: Asset classification (required for all types)

**Asset Types:**

   - ``StaticMesh``: Static mesh only, no animation
   - ``Blueprint``: Blueprint actor with internal animation
   - ``SM+AnimSeq``: Static mesh with external animation sequence

Category Naming Convention
--------------------------

Foreground Categories
~~~~~~~~~~~~~~~~~~~~

.. code-block::

   Foreground_Human        # Human characters
   Foreground_Pet          # Animal companions
   Foreground_Vehicle     # Cars, motorcycles
   Foreground_Object      # Props and objects
   Foreground_*           # Custom categories

Occluder Categories
~~~~~~~~~~~~~~~~~~

.. code-block::

   Occluder_All            # All occluders
   Occluder_Small          # Small objects (chairs, lamps)
   Occluder_Medium         # Medium objects (tables, bushes)
   Occluder_Large          # Large objects (cars, trees)
   Occluder_Human          # Human occluders
   Occluder_*              # Custom categories

Scene Categories
~~~~~~~~~~~~~~~~

.. code-block::

   Scene_Indoor            # Indoor environments
   Scene_Outdoor           # Outdoor environments
   Scene_*                 # Custom categories

API Reference
-------------

GetInstance
~~~~~~~~~~

.. code-block:: cpp

   static FAssetPoolManager& Get();

Gets the singleton instance.

**Returns:** Reference to the singleton instance

LoadStableAssetsPack
~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   void LoadStableAssetsPack();

Loads the predefined stable assets pack.

**Note:** Hardcoded asset paths for Phase 1. Future versions will migrate to JSON/INI config.

GetRandomAsset
~~~~~~~~~~~~~

.. code-block:: cpp

   FString GetRandomAsset(const FString& Category);

Selects a random asset path from a category.

**Parameters:**

   - ``Category`` (const FString&): Category name

**Returns:** Random asset path, or empty string if category not found

GetRandomAssetMetadata
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   TMap<FString, FString> GetRandomAssetMetadata(const FString& Category);

Selects a random asset with full metadata.

**Parameters:**

   - ``Category`` (const FString&): Category name

**Returns:** Complete metadata map for random asset

GetAssetsInCategory
~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   TArray<FString> GetAssetsInCategory(const FString& Category) const;

Gets all asset paths in a category.

**Parameters:**

   - ``Category`` (const FString&): Category name

**Returns:** Array of asset paths

HasCategory
~~~~~~~~~~

.. code-block:: cpp

   bool HasCategory(const FString& Category) const;

Checks if a category exists.

**Parameters:**

   - ``Category`` (const FString&): Category name

**Returns:** ``true`` if category exists

GetAllCategories
~~~~~~~~~~~~~~~

.. code-block:: cpp

   TArray<FString> GetAllCategories() const;

Gets all registered categories.

**Returns:** Array of all category names

GetAssetCount
~~~~~~~~~~~~

.. code-block:: cpp

   int32 GetAssetCount(const FString& Category) const;

Gets the count of assets in a category.

**Parameters:**

   - ``Category`` (const FString&): Category name

**Returns:** Number of assets in category

GetAssetMetadataByPath
~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   TMap<FString, FString> GetAssetMetadataByPath(const FString& AssetPath) const;

Gets metadata for a specific asset path.

**Parameters:**

   - ``AssetPath`` (const FString&): Asset path

**Returns:** Metadata map, or empty if not found

GetCategoryByAssetPath
~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   FString GetCategoryByAssetPath(const FString& AssetPath) const;

Finds the category for an asset.

**Parameters:**

   - ``AssetPath`` (const FString&): Asset path

**Returns:** Category name, or empty string if not found

Registration Functions
---------------------

RegisterAsset
~~~~~~~~~~~~

.. code-block:: cpp

   void RegisterAsset(const FString& Category, const FString& AssetPath);

Registers an asset with minimal metadata.

**Parameters:**

   - ``Category`` (const FString&): Category to register in
   - ``AssetPath`` (const FString&): Asset path

**Note:** Infers type from asset. Does not set animation sequence.

RegisterAssetWithMetadata
~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   void RegisterAssetWithMetadata(const FString& Category, const TMap<FString, FString>& Metadata);

Registers an asset with complete metadata.

**Parameters:**

   - ``Category`` (const FString&): Category to register in
   - ``Metadata`` (const TMap<FString, FString>&): Full metadata map

ValidateMetadata
~~~~~~~~~~~~~~~

.. code-block:: cpp

   static bool ValidateMetadata(const TMap<FString, FString>& Metadata, FString& OutErrorMessage);

Validates asset metadata.

**Parameters:**

   - ``Metadata`` (const TMap<FString, FString>&): Metadata to validate
   - ``OutErrorMessage`` (FString&): Error description if validation fails

**Returns:** ``true`` if metadata is valid

Debug Functions
-------------

PrintAllAssets
~~~~~~~~~~~~~

.. code-block:: cpp

   void PrintAllAssets() const;

Logs all registered assets to output.

PrintAssetPoolSummary
~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   void PrintAssetPoolSummary() const;

Logs category and asset counts.

Integration with PakHandler
--------------------------

The Asset Pool System integrates with PakHandler for dynamic asset loading:

**Workflow:**

1. Mount PAK file via ``vset /pak/mount [PakPath]``
2. Scan for assets via ``vset /pak/scan [PackagePath]``
3. Register assets with metadata via ``vset /pak/register [PackagePath] [Category]``
4. SceneCompositionBPLib queries asset pool for random selection

**PakHandler Commands:**

- ``vget /pak/list`` - List mounted PAK files
- ``vset /pak/mount [PakPath]`` - Mount a PAK file
- ``vset /pak/unmount [PakPath]`` - Unmount a PAK file
- ``vset /pak/register [PackagePath] [Category]`` - Register asset with category
- ``vset /pak/unregister [PackagePath]`` - Remove asset from pool

See Also
--------

- :doc:`../reference/pak-commands` - PakHandler command reference
- :doc:`scene-composition-bplib` - SceneCompositionBPLib API
- :doc:`dataset-generation` - Dataset generation workflow
