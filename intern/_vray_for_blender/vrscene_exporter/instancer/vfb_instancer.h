/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
 *
 * * ***** END GPL LICENSE BLOCK *****
 */

#ifndef CGR_VFB_INSTANCER_H
#define CGR_VFB_INSTANCER_H

#include "exp_types.h"
#include "cgr_hash.h"

namespace VRayForBlender {

/// Resets max. particle ID counter.
void resetMaxParticleID();

/// Returns max. particle ID counter.
MHash getMaxParticleID();

/// Returns unique particle ID. Used for duplication and particles.
/// @param dupliGenerator Dupli generator.
/// @param dupliObject DupliObject instance.
/// @param dupliIndex Dupli index.
MHash getParticleID(BL::Object dupliGenerator, BL::DupliObject dupliObject, int dupliIndex);

/// Returns unique particle ID. Used for Array modifier particles.
/// @param arrayGenerator Array generator.
/// @param arrayIndex Array index.
MHash getParticleID(BL::Object arrayGenerator, int arrayIndex);

///
/// Instancer's "instances" parameter format.
///
///	"use_additional_params" = true;
///	"use_time_instancing" = false;
///
/// "instances"= List(<time>,
///                   List(<id>,
///                        <tm>,
///                        <vel>,
///                        <attr_flags>,
///                        <attr_objectID>,
///                        <attr_primaryVisibility>,
///                        <attr_userAttributes>,
///                        <visible>,
///                        <material>,
///                        <geometry>,
///                        <node>),
///                   List(...)
///              );
///

/// Flags for <attr_flags> list item.
enum InstancerAdditionalParameters {
	useParentTimes                   = (1<<0),
	useObjectID                      = (1<<1),
	usePrimaryVisibility             = (1<<2),
	useUserAttributes                = (1<<3),
	useParentTimesAtleastForGeometry = (1<<4),
	useMaterial                      = (1<<5),
	useGeometry                      = (1<<6),
};

/// Describes Instancer particle.
struct InstancerItem {
	enum InstancerItemDefaults {
		useOriginalObjectID = -1,
	};

	InstancerItem(MHash particleID=0)
	    : particleID(particleID)
	    , flags(useObjectID)
	{}

	/// Unique particle ID.
	MHash particleID;

	/// List item flags.
	int flags;

	/// Duplicated object.
	std::string objectName;

	/// Material override.
	std::string materialOverride;

	/// Object ID override.
	int objectID;

	/// Sets particle transform.
	/// @param tm Dupli transform.
	/// @param obTm Duplicated object transform.
	void setTransform(const BLTransform &dupliTransform, const BLTransform &objectTransform);

	/// Sets particle transform.
	/// @param tm Dupli transform.
	/// @param obTm Duplicated object transform.
	void setTransform(float dupliTransform[4][4], float objectTransform[4][4]);

	/// Returns particle transform.
	const BLTransform &getTransform() const { return tm; }

private:
	/// Particle transform.
	BLTransform tm;
};

/// An array of InstancerItem.
typedef std::vector<InstancerItem> InstancerItems;

/// Instancer particles storage.
class Instancer {
public:
	/// Appends new particle.
	void addParticle(const InstancerItem &item);

	/// Returns items array.
	const InstancerItems &getParticles() const { return items; }

	/// Clears particles array.
	void freeData();

private:
	/// Particles array.
	InstancerItems items;
};

} // namespace VRayForBlender

#endif // CGR_VFB_INSTANCER_H
