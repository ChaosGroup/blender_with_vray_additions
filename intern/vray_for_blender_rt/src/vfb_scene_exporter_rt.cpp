/*
 * Copyright (c) 2015, Chaos Software Ltd
 *
 * V-Ray For Blender
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "vfb_scene_exporter_rt.h"

#ifdef WITH_GLEW_MX
#  include "glew-mx.h"
#else
#  include <GL/glew.h>
#  define mxCreateContext() glewInit()
#  define mxMakeCurrentContext(x) (x)
#endif


void InteractiveExporter::create_exporter()
{
	if (m_settings.exporter_type == ExpoterType::ExpoterTypeFile) {
#if defined(USE_BLENDER_VRAY_APPSDK)
		m_settings.exporter_type = ExpoterType::ExpoterTypeAppSDK;
#else
		m_settings.exporter_type = ExpoterType::ExpoterTypeZMQ;
#endif
	}

	SceneExporter::create_exporter();

	if (m_exporter) {
		m_exporter->set_is_viewport(true);
		m_exporter->set_settings(m_settings);
	}
}


void InteractiveExporter::setup_callbacks()
{
	m_exporter->set_callback_on_image_ready(ExpoterCallback(boost::bind(&InteractiveExporter::cb_on_image_ready, this)));
	m_exporter->set_callback_on_rt_image_updated(ExpoterCallback(boost::bind(&InteractiveExporter::cb_on_image_ready, this)));
}


void InteractiveExporter::sync_dupli(BL::Object ob, const int &check_updated)
{
	ob.dupli_list_create(m_scene, EvalMode::EvalModePreview);

	SceneExporter::sync_dupli(ob, check_updated);

	ob.dupli_list_clear();
}


void InteractiveExporter::sync_object(BL::Object ob, const int &check_updated, const ObjectOverridesAttrs &override)
{
	BL::Object::modifiers_iterator modIt;
	SceneExporter::sync_object(ob, check_updated, override);

	for (ob.modifiers.begin(modIt); modIt != ob.modifiers.end(); ++modIt) {
		BL::Modifier mod(*modIt);
		if (mod && mod.show_render() && mod.type() == BL::Modifier::type_PARTICLE_SYSTEM) {
			BL::ParticleSystemModifier psm(mod);
			BL::ParticleSystem psys = psm.particle_system();
			if (psys) {
				m_data_exporter.exportHair(ob, psm, psys, check_updated);
			}
		}
	}
}


void InteractiveExporter::draw()
{
	sync_view(true);

	RenderImage image = m_exporter->get_image();
	if (!image) {
		tag_redraw();
	}
	else {
		const bool transparent = m_settings.getViewportShowAlpha();

		glPushMatrix();
		glTranslatef(m_viewParams.viewport_offs_x, m_viewParams.viewport_offs_y, 0.0f);

		if (transparent) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		}
		else {
			image.resetAlpha();
		}
		image.clamp();

		glColor3f(1.0f, 1.0f, 1.0f);

		GLuint texid;
		glGenTextures(1, &texid);
		glBindTexture(GL_TEXTURE_2D, texid);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F_ARB, image.w, image.h, 0, GL_RGBA, GL_FLOAT, image.pixels);

		const int glFilter = (m_viewParams.viewport_w == m_viewParams.renderSize.w)
		                     ? GL_NEAREST
		                     : GL_LINEAR;

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glFilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glFilter);

		glEnable(GL_TEXTURE_2D);

		glPushMatrix();
		glTranslatef(0.0f, 0.0f, 0.0f);

		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 1.0f);
		glVertex2f(0.0f, 0.0f);
		glTexCoord2f(1.0f, 1.0f);
		glVertex2f((float)m_viewParams.viewport_w, 0.0f);
		glTexCoord2f(1.0f, 0.0f);
		glVertex2f((float)m_viewParams.viewport_w, (float)m_viewParams.viewport_h);
		glTexCoord2f(0.0f, 0.0f);
		glVertex2f(0.0f, (float)m_viewParams.viewport_h);
		glEnd();

		glPopMatrix();

		glBindTexture(GL_TEXTURE_2D, 0);
		glDisable(GL_TEXTURE_2D);
		glDeleteTextures(1, &texid);

		if (transparent) {
			glDisable(GL_BLEND);
		}

		glPopMatrix();

		image.free();
	}
}
