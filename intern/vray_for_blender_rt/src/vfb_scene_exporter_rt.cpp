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
	if (m_settings.exporter_type == ExporterType::ExpoterTypeFile) {
		m_settings.exporter_type = ExporterType::ExpoterTypeZMQ;
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


bool InteractiveExporter::export_scene(const bool check_updated)
{
	clock_t begin = clock();

	m_frameExporter.updateFromSettings();

	m_frameExporter.forEachFrameInBatch([this, check_updated](FrameExportManager & frameExp) {
		if (m_scene.frame_current() != frameExp.getSceneFrameToExport()) {
			m_scene.frame_set(frameExp.getSceneFrameToExport(), 0.f);
		}
		m_settings.update(m_context, m_engine, m_data, m_scene);
		// set the frame to export (so values are inserted for that time)
		m_exporter->set_current_frame(m_frameExporter.getSceneFrameToExport());
		sync(check_updated);
		return true;
	});

	m_frameExporter.reset();

	// Export stuff after sync
	if (m_settings.work_mode == ExporterSettings::WorkMode::WorkModeExportOnly ||
		m_settings.work_mode == ExporterSettings::WorkMode::WorkModeRenderAndExport) {
		const std::string filepath = "scene_app_sdk.vrscene";
		m_exporter->export_vrscene(filepath);
	}

	// finally set the frame that we want to render to so we actually render the correct frame
	m_exporter->set_current_frame(m_frameExporter.getCurrentRenderFrame());

	if (m_exporter->get_commit_state() != VRayBaseTypes::CommitAction::CommitAutoOn) {
		m_exporter->commit_changes();
	}

	clock_t end = clock();
	double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;
	PRINT_INFO_EX("Synced in %.3f sec.", elapsed_secs);

	return true;
}


void InteractiveExporter::sync_dupli(BL::Object ob, const int &check_updated)
{
	ob.dupli_list_create(m_scene, EvalMode::EvalModePreview);

	SceneExporter::sync_dupli(ob, check_updated);

	ob.dupli_list_clear();
}

void InteractiveExporter::draw()
{
	// TODO: is it worth it here to let python run for sync_view
	// python_thread_state_save();
	sync_view(true);
	// python_thread_state_restore();

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

		if (glGetError() != GL_NO_ERROR) {
			// for some reason we cant draw 2d textures

			glBindTexture(GL_TEXTURE_2D, 0);
			glDisable(GL_TEXTURE_2D);
			glDeleteTextures(1, &texid);

			glPixelZoom((float)m_viewParams.viewport_w/(float)image.w, (float)m_viewParams.viewport_h/(float)image.h);
			glRasterPos2f(0.f, 0.f);

			// we need to manually flip since we cant do it on the device
			image.flip();
			glDrawPixels(image.w, image.h, GL_RGBA, GL_FLOAT, image.pixels);

			glPixelZoom(1.0f, 1.0f);
		}
		else {
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
		}

		if (transparent) {
			glDisable(GL_BLEND);
		}

		glPopMatrix();
	}
}
