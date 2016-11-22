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

#include "vfb_instancer.h"

#include <BLI_math_matrix.h>

using namespace VRayForBlender;

MHash VRayForBlender::getParticleID(BL::Object dupliGenerator, BL::DupliObject dupliObject, int dupliIndex)
{
	MHash particleID = 0;

	/// Unique particle ID.
	/// Combination of persistent_id(), particle index, dupli index and objects pointers.
	struct ParticleKey {
		ParticleKey(BL::Object dupliGenerator, BL::DupliObject dupliObject, int dupliIndex) {
			::memset(key, 0, sizeof(key));

			int keyOffs = 0;

			// Particle index.
			key[keyOffs++] = dupliObject.index();

			// Dupli index.
			key[keyOffs++] = dupliIndex;

			// Duplicated object pointer.
			key[keyOffs++] = reinterpret_cast<intptr_t>(dupliObject.object().ptr.data);

			// Generator pointer.
			key[keyOffs++] = reinterpret_cast<intptr_t>(dupliGenerator.ptr.data);

			// Persistent ID.
			::memcpy(key+keyOffs, dupliObject.persistent_id().data, 16 * sizeof(int));
		}

		intptr_t key[20];
	} particleKey(dupliGenerator, dupliObject, dupliIndex);

	MurmurHash3_x86_32((const void*)&particleKey, sizeof(ParticleKey), 42, &particleID);

	return particleID;
}

MHash VRayForBlender::getParticleID(BL::Object arrayGenerator, int arrayIndex)
{
	MHash particleID = 0;

	/// Unique particle ID.
	/// Combination of persistent_id(), particle index, dupli index and objects pointers.
	struct ArrayKey {
		ArrayKey(BL::Object arrayGenerator, int arrayIndex) {
			::memset(key, 0, sizeof(key));

			int keyOffs = 0;

			// Array index.
			key[keyOffs++] = arrayIndex;

			// Array generator object pointer.
			key[keyOffs++] = reinterpret_cast<intptr_t>(arrayGenerator.ptr.data);
		}

		intptr_t key[20];
	} arrayKey(arrayGenerator, arrayIndex);

	MurmurHash3_x86_32((const void*)&arrayKey, sizeof(ArrayKey), 42, &particleID);

	return particleID;
}

void Instancer::addParticle(const InstancerItem &item)
{
	items.push_back(item);
}

void Instancer::freeData()
{
	items.clear();
}

void InstancerItem::setTransform(float dupliTransform[4][4], float objectTransform[4][4])
{
	// Instancer use original object's transform,
	// so apply inverse matrix here.
	// When linking from file 'imat' is not valid,
	// so better to always calculate inverse matrix ourselves.
	float obTmInv[4][4];
	copy_m4_m4(obTmInv, objectTransform);
	invert_m4(obTmInv);

	// Final Intancer particle transform.
	float particleTm[4][4];
	mul_m4_m4m4(particleTm, dupliTransform, obTmInv);

	::memcpy((void*)tm.data, (const void*)particleTm, 16 * sizeof(float));
}

void InstancerItem::setTransform(const BLTransform &dupliTransform, const BLTransform &objectTransform)
{
	// Dupli tm.
	float dupTm[4][4];
	::memcpy(dupTm, dupliTransform.data, 16 * sizeof(float));

	// Original object tm.
	float origObTm[4][4];
	::memcpy(origObTm, objectTransform.data, 16 * sizeof(float));

	setTransform(dupTm, origObTm);
}
