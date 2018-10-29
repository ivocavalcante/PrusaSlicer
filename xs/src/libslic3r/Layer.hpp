#ifndef slic3r_Layer_hpp_
#define slic3r_Layer_hpp_

#include "libslic3r.h"
#include "Flow.hpp"
#include "SurfaceCollection.hpp"
#include "ExtrusionEntityCollection.hpp"
#include "ExPolygonCollection.hpp"
#include "PolylineCollection.hpp"


namespace Slic3r {

class Layer;
class PrintRegion;
class PrintObject;

// TODO: make stuff private
class LayerRegion
{
    friend class Layer;

public:
    Layer*              layer()        { return this->_layer; }
    const Layer*        layer()  const { return this->_layer; }
    PrintRegion*        region()       { return this->_region; }
    const PrintRegion*  region() const { return this->_region; }

    // Collection of surfaces generated by slicing the original geometry, divided by type top/bottom/internal.
    SurfaceCollection           slices;

    // Unspecified fill polygons, used for overhang detection ("ensure vertical wall thickness feature")
    // and for re-starting of infills.
    ExPolygons                  fill_expolygons;
    // Unspecified fill polygons, used for interecting when we don't want the infill/perimeter overlap
    ExPolygons          fill_no_overlap_expolygons;
    // collection of surfaces for infill generation
    SurfaceCollection           fill_surfaces;
    // Collection of extrusion paths/loops filling gaps.
    // These fills are generated by the perimeter generator.
    // They are not printed on their own, but they are copied to this->fills during infill generation.
    ExtrusionEntityCollection   thin_fills;

    // Collection of expolygons representing the bridged areas (thus not needing support material).
    //FIXME Not used as of now.
    Polygons                    bridged;

    // collection of polylines representing the unsupported bridge edges
    PolylineCollection          unsupported_bridge_edges;

    // Ordered collection of extrusion paths/loops to build all perimeters.
    // This collection contains only ExtrusionEntityCollection objects.
    ExtrusionEntityCollection   perimeters;
    // Ordered collection of extrusion paths to fill surfaces.
    // This collection contains only ExtrusionEntityCollection objects.
    ExtrusionEntityCollection   fills;
    
    Flow flow(FlowRole role, bool bridge = false, double width = -1) const;
    void slices_to_fill_surfaces_clipped();
    void prepare_fill_surfaces();
    void make_perimeters(const SurfaceCollection &slices, SurfaceCollection* fill_surfaces);
    void process_external_surfaces(const Layer* lower_layer);
    double infill_area_threshold() const;

    void export_region_slices_to_svg(const char *path) const;
    void export_region_fill_surfaces_to_svg(const char *path) const;
    // Export to "out/LayerRegion-name-%d.svg" with an increasing index with every export.
    void export_region_slices_to_svg_debug(const char *name) const;
    void export_region_fill_surfaces_to_svg_debug(const char *name) const;

    // Is there any valid extrusion assigned to this LayerRegion?
    bool has_extrusions() const { return ! this->perimeters.entities.empty() || ! this->fills.entities.empty(); }

private:
    Layer *_layer;
    PrintRegion *_region;

    LayerRegion(Layer *layer, PrintRegion *region) : _layer(layer), _region(region) {}
    ~LayerRegion() {}
};


typedef std::vector<LayerRegion*> LayerRegionPtrs;

class Layer {
    friend class PrintObject;

public:
    size_t id() const { return this->_id; }
    void set_id(size_t id) { this->_id = id; }
    PrintObject* object() { return this->_object; }
    const PrintObject* object() const { return this->_object; }

    Layer *upper_layer;
    Layer *lower_layer;
    LayerRegionPtrs regions;
    bool slicing_errors;
    coordf_t slice_z;       // Z used for slicing in unscaled coordinates
    coordf_t print_z;       // Z used for printing in unscaled coordinates
    coordf_t height;        // layer height in unscaled coordinates

    // collection of expolygons generated by slicing the original geometry;
    // also known as 'islands' (all regions and surface types are merged here)
    // The slices are chained by the shortest traverse distance and this traversal
    // order will be recovered by the G-code generator.
    ExPolygonCollection slices;

    size_t region_count() const { return this->regions.size(); }
    const LayerRegion* get_region(int idx) const { return this->regions.at(idx); }
    LayerRegion* get_region(int idx) { return this->regions.at(idx); }
    LayerRegion* add_region(PrintRegion* print_region);
    
    void make_slices();
    void merge_slices();
    template <class T> bool any_internal_region_slice_contains(const T &item) const {
        for (const LayerRegion *layerm : this->regions) if (layerm->slices.any_internal_contains(item)) return true;
        return false;
    }
    template <class T> bool any_bottom_region_slice_contains(const T &item) const {
        for (const LayerRegion *layerm : this->regions) if (layerm->slices.any_bottom_contains(item)) return true;
        return false;
    }
    void make_perimeters();
    void make_fills();

    void export_region_slices_to_svg(const char *path) const;
    void export_region_fill_surfaces_to_svg(const char *path) const;
    // Export to "out/LayerRegion-name-%d.svg" with an increasing index with every export.
    void export_region_slices_to_svg_debug(const char *name) const;
    void export_region_fill_surfaces_to_svg_debug(const char *name) const;

    // Is there any valid extrusion assigned to this LayerRegion?
    virtual bool has_extrusions() const { for (auto layerm : this->regions) if (layerm->has_extrusions()) return true; return false; }

protected:
    size_t _id;     // sequential number of layer, 0-based
    PrintObject *_object;

    Layer(size_t id, PrintObject *object, coordf_t height, coordf_t print_z, coordf_t slice_z) :
        upper_layer(nullptr), lower_layer(nullptr), slicing_errors(false),
        slice_z(slice_z), print_z(print_z), height(height),
        _id(id), _object(object) {}
    virtual ~Layer();
};

class SupportLayer : public Layer {
    friend class PrintObject;

public:
    // Polygons covered by the supports: base, interface and contact areas.
    ExPolygonCollection support_islands;
    // Extrusion paths for the support base and for the support interface and contacts.
    ExtrusionEntityCollection support_fills;

    // Is there any valid extrusion assigned to this LayerRegion?
    virtual bool has_extrusions() const { return ! support_fills.empty(); }

//protected:
    // The constructor has been made public to be able to insert additional support layers for the skirt or a wipe tower
    // between the raft and the object first layer.
    SupportLayer(size_t id, PrintObject *object, coordf_t height, coordf_t print_z, coordf_t slice_z) :
        Layer(id, object, height, print_z, slice_z) {}
    virtual ~SupportLayer() {}
};

}

#endif
