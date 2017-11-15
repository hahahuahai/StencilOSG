#include <osg\Node>
#include <osg\Geometry>
#include <osg\Geode>
#include <osg\Notify>
#include <osg\MatrixTransform>
#include <osg\Texture2D>
#include <osg\BlendFunc>
#include <osg\Stencil>
#include <osg\ColorMask>
#include <osg/Depth>
#include <osg/ClipNode>
#include <osg/AnimationPath>

#include <osgDB/ReadFile>
#include <osgDB/WriteFile>

#include <osgUtil/Optimizer>
#include <osgViewer/Viewer>

#include <iostream>


osg::StateSet* createMirrorTexturedState(const std::string& filename)
{
	osg::StateSet* dstate = new osg::StateSet;
	dstate->setMode(GL_CULL_FACE, osg::StateAttribute::OFF | osg::StateAttribute::PROTECTED);

	// set up the texture.
	osg::ref_ptr<osg::Image> image = osgDB::readRefImageFile(filename.c_str());
	if (image)
	{
		osg::Texture2D* texture = new osg::Texture2D;
		texture->setImage(image);
		dstate->setTextureAttributeAndModes(0, texture, osg::StateAttribute::ON | osg::StateAttribute::PROTECTED);
	}

	return dstate;
}


osg::Drawable* createMirrorSurface(float xMin, float xMax, float yMin, float yMax, float z)
{
	osg::Geometry* geom = new osg::Geometry;

	osg::Vec3Array* coords = new osg::Vec3Array(4);
	(*coords)[0].set(xMin, yMax, z);
	(*coords)[1].set(xMin, yMin, z);
	(*coords)[2].set(xMax, yMin, z);
	(*coords)[3].set(xMax, yMax, z);
	geom->setVertexArray(coords);

	osg::Vec3Array* norms = new osg::Vec3Array(1);
	(*norms)[0].set(0.0f, 0.0f, 1.0f);
	geom->setNormalArray(norms, osg::Array::BIND_OVERALL);

	osg::Vec2Array* tcoords = new osg::Vec2Array(4);
	(*tcoords)[0].set(0.0f, 1.0f);
	(*tcoords)[1].set(0.0f, 0.0f);
	(*tcoords)[2].set(1.0f, 0.0f);
	(*tcoords)[3].set(1.0f, 1.0f);
	geom->setTexCoordArray(0, tcoords);

	osg::Vec4Array* colours = new osg::Vec4Array(1);
	(*colours)[0].set(1.0f, 1.0f, 1.0, 1.0f);
	geom->setColorArray(colours, osg::Array::BIND_OVERALL);

	geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::QUADS, 0, 4));

	return geom;
}


osg::Node* createMirroredScene(osg::Node* model)
{
	const osg::BoundingSphere& bs = model->getBound();

	float width_factor = 1.5;
	float height_factor = 0.3;

	float xMin = bs.center().x() - bs.radius()*width_factor;
	float xMax = bs.center().x() + bs.radius()*width_factor;
	float yMin = bs.center().y() - bs.radius()*width_factor;
	float yMax = bs.center().y() + bs.radius()*width_factor;

	float z = bs.center().z() - bs.radius()*height_factor;

	osg::Drawable* mirror = createMirrorSurface(xMin, xMax, yMin, yMax, z);

	osg::MatrixTransform* rootNode = new osg::MatrixTransform;
	rootNode->setMatrix(osg::Matrix::rotate(osg::inDegrees(45.0f), 1.0f, 0.0f, 0.0f));

	osg::ColorMask* rootColorMask = new osg::ColorMask;
	rootColorMask->setMask(true, true, true, true);

	osg::Depth* rootDepth = new osg::Depth;
	rootDepth->setFunction(osg::Depth::LESS);
	rootDepth->setRange(0.0, 1.0);

	osg::StateSet* rootStateSet = new osg::StateSet();
	rootStateSet->setAttribute(rootColorMask);
	rootStateSet->setAttribute(rootDepth);

	rootNode->setStateSet(rootStateSet);

	// bin1  - set up the stencil values and depth for mirror.
	{
		osg::Stencil* stencil = new osg::Stencil;
		stencil->setFunction(osg::Stencil::ALWAYS, 1, ~0u);
		stencil->setOperation(osg::Stencil::KEEP, osg::Stencil::KEEP, osg::Stencil::REPLACE);

		osg::ColorMask* colorMask = new osg::ColorMask;
		colorMask->setMask(false, false, false, false);

		osg::StateSet* statesetBin1 = new osg::StateSet();
		statesetBin1->setRenderBinDetails(1, "RenderBin");
		statesetBin1->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);
		statesetBin1->setAttributeAndModes(stencil, osg::StateAttribute::ON);
		statesetBin1->setAttribute(colorMask);

		osg::Geode* geode = new osg::Geode;
		geode->addDrawable(mirror);
		geode->setStateSet(statesetBin1);

		rootNode->addChild(geode);
	}
	//bin2
	{
		osg::Stencil* stencil = new osg::Stencil;
		stencil->setFunction(osg::Stencil::ALWAYS, 0, ~0u);
		stencil->setOperation(osg::Stencil::KEEP, osg::Stencil::KEEP, osg::Stencil::REPLACE);

		osg::StateSet* statesetBin2 = new osg::StateSet();
		statesetBin2->setRenderBinDetails(2, "RenderBin");
		statesetBin2->setAttributeAndModes(stencil, osg::StateAttribute::ON);

		osg::Group* groupBin2 = new osg::Group();
		groupBin2->setStateSet(statesetBin2);
		groupBin2->addChild(model);

		rootNode->addChild(groupBin2);
	}

	//bin3
	{
		osg::Stencil* stencil = new osg::Stencil;
		stencil->setFunction(osg::Stencil::EQUAL, 1, ~0u);
		stencil->setOperation(osg::Stencil::KEEP, osg::Stencil::KEEP, osg::Stencil::KEEP);

		osg::ColorMask* colorMask = new osg::ColorMask;
		colorMask->setMask(false, false, false, false);

		osg::Depth* depth = new osg::Depth;
		depth->setFunction(osg::Depth::ALWAYS);
		depth->setRange(1.0, 1.0);

		osg::StateSet* statesetBin3 = new osg::StateSet();
		statesetBin3->setRenderBinDetails(3, "RenderBin");
		statesetBin3->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);
		statesetBin3->setAttributeAndModes(stencil, osg::StateAttribute::ON);
		statesetBin3->setAttribute(colorMask);
		statesetBin3->setAttribute(depth);

		osg::Geode* geode = new osg::Geode;
		geode->addDrawable(mirror);
		geode->setStateSet(statesetBin3);

		rootNode->addChild(geode);
	}

	//bin4
	{
		osg::ClipPlane* clipplane = new osg::ClipPlane;
		clipplane->setClipPlane(0.0, 0.0, -1.0, z);
		clipplane->setClipPlaneNum(0);

		osg::ClipNode* clipNode = new osg::ClipNode;
		clipNode->addClipPlane(clipplane);

		osg::StateSet* dstate = clipNode->getOrCreateStateSet();
		dstate->setRenderBinDetails(4, "RenderBin");
		dstate->setMode(GL_CULL_FACE, osg::StateAttribute::OVERRIDE | osg::StateAttribute::OFF);

		osg::Stencil* stencil = new osg::Stencil;
		stencil->setFunction(osg::Stencil::EQUAL, 1, ~0u);
		stencil->setOperation(osg::Stencil::KEEP, osg::Stencil::KEEP, osg::Stencil::KEEP);
		dstate->setAttributeAndModes(stencil, osg::StateAttribute::ON);

		osg::MatrixTransform* reverseMatrix = new osg::MatrixTransform;
		reverseMatrix->setStateSet(dstate);
		reverseMatrix->preMult(osg::Matrix::translate(0.0f, 0.0f, -z)*
			osg::Matrix::scale(1.0f, 1.0f, -1.0f)*
			osg::Matrix::translate(0.0f, 0.0f, z));

		reverseMatrix->addChild(model);

		clipNode->addChild(reverseMatrix);

		rootNode->addChild(clipNode);
	}

	//bin5
	{
		osg::Depth* depth = new osg::Depth;
		depth->setFunction(osg::Depth::ALWAYS);

		osg::Stencil* stencil = new osg::Stencil;
		stencil->setFunction(osg::Stencil::EQUAL, 1, ~0u);
		stencil->setOperation(osg::Stencil::KEEP, osg::Stencil::KEEP, osg::Stencil::ZERO);

		osg::BlendFunc* trans = new osg::BlendFunc;
		trans->setFunction(osg::BlendFunc::ONE, osg::BlendFunc::ONE);

		osg::StateSet* statesetBin5 = createMirrorTexturedState("tank.rgb");

		statesetBin5->setRenderBinDetails(5, "RenderBin");
		statesetBin5->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);
		statesetBin5->setAttributeAndModes(stencil, osg::StateAttribute::ON);
		statesetBin5->setAttributeAndModes(trans, osg::StateAttribute::ON);
		statesetBin5->setAttribute(depth);

		// set up the mirror geode.
		osg::Geode* geode = new osg::Geode;
		geode->addDrawable(mirror);
		geode->setStateSet(statesetBin5);

		rootNode->addChild(geode);
	}

	return rootNode;
}

int main()
{
	osgViewer::Viewer viewer;

	osg::ref_ptr<osg::Node> loadedModel = osgDB::readRefNodeFile("cessna.osgt");

	osgUtil::Optimizer optimizer;
	optimizer.optimize(loadedModel);

	osg::ref_ptr<osg::MatrixTransform> loadedModelTransform = new osg::MatrixTransform;
	loadedModelTransform->addChild(loadedModel);

	osg::ref_ptr<osg::NodeCallback> nc = new osg::AnimationPathCallback(loadedModelTransform->getBound().center(), osg::Vec3(0.0f, 0.0f, 1.0f), osg::inDegrees(45.0f));
	loadedModelTransform->setUpdateCallback(nc.get());

	osg::ref_ptr<osg::Node> rootNode = createMirroredScene(loadedModelTransform.get());

	viewer.setSceneData(rootNode);

	osg::DisplaySettings::instance()->setMinimumNumStencilBits(8);

	return viewer.run();
}