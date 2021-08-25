/*
 * Copyright (C) 2021 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include "VisualizationCapabilities.hh"

#include <ignition/msgs/boolean.pb.h>
#include <ignition/msgs/stringmsg.pb.h>

#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <stack>
#include <string>
#include <utility>
#include <vector>

#include <ignition/common/Console.hh>
#include <ignition/common/HeightmapData.hh>
#include <ignition/common/ImageHeightmap.hh>
#include <ignition/common/MeshManager.hh>
#include <ignition/common/Profiler.hh>

#include <ignition/gui/Application.hh>
#include <ignition/gui/GuiEvents.hh>
#include <ignition/gui/Helpers.hh>
#include <ignition/gui/MainWindow.hh>

#include <ignition/plugin/Register.hh>

#include "ignition/rendering/Capsule.hh"
#include <ignition/rendering/COMVisual.hh>
#include <ignition/rendering/Heightmap.hh>
#include <ignition/rendering/InertiaVisual.hh>
#include <ignition/rendering/Visual.hh>
#include <ignition/rendering/RenderingIface.hh>
#include <ignition/rendering/Scene.hh>
#include <ignition/rendering/WireBox.hh>

#include <ignition/transport/Node.hh>

#include <sdf/Capsule.hh>
#include <sdf/Ellipsoid.hh>
#include <sdf/Heightmap.hh>
#include <sdf/Mesh.hh>
#include <sdf/Pbr.hh>
#include <sdf/Root.hh>

#include "ignition/gazebo/components/CastShadows.hh"
#include "ignition/gazebo/components/Collision.hh"
#include "ignition/gazebo/components/Geometry.hh"
#include "ignition/gazebo/components/Inertial.hh"
#include "ignition/gazebo/components/Link.hh"
#include "ignition/gazebo/components/Model.hh"
#include "ignition/gazebo/components/Name.hh"
#include "ignition/gazebo/components/ParentEntity.hh"
#include "ignition/gazebo/components/Pose.hh"
#include "ignition/gazebo/components/Transparency.hh"
#include "ignition/gazebo/components/Visibility.hh"
#include "ignition/gazebo/components/Visual.hh"

#include "ignition/gazebo/Util.hh"
#include "ignition/gazebo/rendering/RenderUtil.hh"
#include "ignition/gazebo/rendering/SceneManager.hh"

namespace ignition::gazebo
{
  class VisualizationCapabilitiesPrivate
  {
    /// \brief Update the 3D scene.
    public: void OnRender();

    /// \brief Helper function to get all child links of a model entity.
    /// \param[in] _entity Entity to find child links
    /// \return Vector of child links found for the parent entity
    public: std::vector<ignition::gazebo::Entity>
      FindChildLinks(const ignition::gazebo::Entity &_entity);

    /// \brief Finds the links (collision parent) that are used to create child
    /// collision visuals in RenderUtil::Update
    /// \param[in] _ecm The entity-component manager
    public: void FindCollisionLinks(const EntityComponentManager &_ecm);

    /// \brief Finds the child links for given entity from the ECM
    /// \param[in] _ecm The entity-component manager
    /// \param[in] _entity Entity to find child links
    /// \return A vector of child links found for the entity
    public: std::vector<ignition::gazebo::Entity> FindChildLinksFromECM(
        const ignition::gazebo::EntityComponentManager &_ecm,
        const ignition::gazebo::Entity &_entity);

    /// \brief Finds the links (visual parent) that are used to toggle wireframe
    /// and transparent view for visuals in RenderUtil::Update
    /// \param[in] _ecm The entity-component manager
    public: void PopulateViewModeVisualLinks(
      const ignition::gazebo::EntityComponentManager &_ecm);

    /// \brief Finds the links (inertial parent) that are used to create child
    /// inertia and center of mass visuals in RenderUtil::Update
    /// \param[in] _ecm The entity-component manager
    public: void FindInertialLinks(const EntityComponentManager &_ecm);

    /// \brief Retrieve visual
    /// \param[in] _id Unique visual (entity) id
    /// \return Pointer to requested visual
    public: ignition::rendering::VisualPtr VisualById(unsigned int _id);

    public: rendering::VisualPtr CreateVisual(
      ignition::gazebo::Entity _id,
      const sdf::Visual &_visual,
      rendering::VisualPtr &_parent);

    public: rendering::GeometryPtr LoadGeometry(
        const sdf::Geometry &_geom, math::Vector3d &_scale,
        math::Pose3d &_localPose);

    public: rendering::MaterialPtr LoadMaterial(
        const sdf::Material &_material);

    /////////////////////////////////////////////////
    // Transparent
    /////////////////////////////////////////////////
    /// \brief View an entity as transparent
    /// \param[in] _entity Entity to view as transparent
    public: void ViewTransparent(const ignition::gazebo::Entity &_entity);

    /// \brief Callback for view as transparent request
    /// \param[in] _msg Request message to set the target to view as
    /// transparent
    /// \param[in] _res Response data
    /// \return True if the request is received
    public: bool OnViewTransparent(const msgs::StringMsg &_msg,
      msgs::Boolean &_res);

    /////////////////////////////////////////////////
    // Wireframes
    /////////////////////////////////////////////////
    /// \brief View wireframes of specified entity
    /// \param[in] _entity Entity to view wireframes
    public: void ViewWireframes(const ignition::gazebo::Entity &_entity);

    /// \brief Callback for view wireframes request
    /// \param[in] _msg Request message to set the target to view wireframes
    /// \param[in] _res Response data
    /// \return True if the request is received
    public: bool OnViewWireframes(const msgs::StringMsg &_msg,
      msgs::Boolean &_res);

    /////////////////////////////////////////////////
    // Collision
    /////////////////////////////////////////////////

    /// \brief View collisions of specified entity which are shown in orange
    /// \param[in] _entity Entity to view collisions
    public: void ViewCollisions(const Entity &_entity);

    /// \brief Callback for view collisions request
    /// \param[in] _msg Request message to set the target to view collisions
    /// \param[in] _res Response data
    /// \return True if the request is received
    public: bool OnViewCollisions(const msgs::StringMsg &_msg,
        msgs::Boolean &_res);

    /////////////////////////////////////////////////
    public: rendering::VisualPtr CreateCollision(
      ignition::gazebo::Entity _id,
      const sdf::Collision &_collision,
      ignition::rendering::VisualPtr &_parent);

    /////////////////////////////////////////////////
    // COM
    /////////////////////////////////////////////////
    /// \brief View center of mass of specified entity
    /// \param[in] _entity Entity to view center of mass
    public: void ViewCOM(const Entity &_entity);

    /// \brief Callback for view center of mass request
    /// \param[in] _msg Request message to set the target to view center of
    /// mass
    /// \param[in] _res Response data
    /// \return True if the request is received
    public: bool OnViewCOM(const msgs::StringMsg &_msg,
      msgs::Boolean &_res);

    /// \brief Create a center of mass visual
    /// \param[in] _id Unique visual id
    /// \param[in] _inertial Inertial component of the link
    /// \param[in] _parent Visual parent
    /// \return Visual (center of mass) object created from the inertial
    public: ignition::rendering::VisualPtr createCOMVisual(
      ignition::gazebo::Entity _id,
      const math::Inertiald &_inertia,
      ignition::rendering::VisualPtr &_parent);
    /////////////////////////////////////////////////
    // Inertia
    /////////////////////////////////////////////////
    /// \brief View inertia of specified entity
    /// \param[in] _entity Entity to view inertia
    public: void ViewInertia(const Entity &_entity);

    /// \brief Callback for view inertia request
    /// \param[in] _msg Request message to set the target to view inertia
    /// \param[in] _res Response data
    /// \return True if the request is received
    public: bool OnViewInertia(const msgs::StringMsg &_msg,
      msgs::Boolean &_res);

    /// \brief Create an inertia visual
    /// \param[in] _id Unique visual id
    /// \param[in] _inertial Inertial component of the link
    /// \param[in] _parent Visual parent
    /// \return Visual (center of mass) object created from the inertial
    public: ignition::rendering::VisualPtr CreateInertiaVisual(
      ignition::gazebo::Entity _id,
      const math::Inertiald &_inertia,
      ignition::rendering::VisualPtr &_parent);
    /////////////////////////////////////////////////

    /// \brief Ignition communication node.
    public: transport::Node node;

    /// \brief Pointer to the rendering scene
    public: ignition::rendering::ScenePtr scene{nullptr};

    /// \brief Scene manager
    public: ignition::gazebo::SceneManager sceneManager;

    /// True if the rendering component is initialized
    public: bool initialized = false;

    /// \brief A map of model entities and their corresponding children links
    public: std::map<Entity, std::vector<Entity>> modelToLinkEntities;

    /// \brief A map of model entities and their corresponding children models
    public: std::map<Entity, std::vector<Entity>> modelToModelEntities;

    /// \brief New wireframe visuals to be toggled
    public: std::vector<ignition::gazebo::Entity> newTransparentEntities;

    /// \brief A map of link entities and their corresponding children visuals
    public: std::map<ignition::gazebo::Entity,
      std::vector<ignition::gazebo::Entity>> linkToVisualEntities;

    /// \brief Map of visual entity in Gazebo to visual pointers.
    public: std::map<Entity, rendering::VisualPtr> visuals;

    /////////////////////////////////////////////////
    // Transparent
    /////////////////////////////////////////////////
    /// \brief Target to view as transparent
    public: std::string viewTransparentTarget;

    /// \brief A list of links used to toggle transparent mode for visuals
    public: std::vector<Entity> newTransparentVisualLinks;

    /// \brief A map of created transparent visuals and if they are currently
    /// visible
    public: std::map<ignition::gazebo::Entity, bool> viewingTransparent;

    /// \brief View transparent service
    public: std::string viewTransparentService;

    /////////////////////////////////////////////////
    // Wireframes
    /////////////////////////////////////////////////

    /// \brief Target to view wireframes
    public: std::string viewWireframesTarget;

    /// \brief New wireframe visuals to be toggled
    public: std::vector<Entity> newWireframes;

    /// \brief A map of created wireframe visuals and if they are currently
    /// visible
    public: std::map<Entity, bool> viewingWireframes;

    /// \brief A list of links used to toggle wireframe mode for visuals
    public: std::vector<Entity> newWireframeVisualLinks;

    /// \brief View transparent service
    public: std::string viewWireframesService;

    /////////////////////////////////////////////////
    // COM
    /////////////////////////////////////////////////

    /// \brief New center of mass visuals to be created
    public: std::vector<Entity> newCOMVisuals;

    /// \brief A list of links used to create new center of mass visuals
    public: std::vector<Entity> newCOMLinks;

    /// \brief A map of link entities and if their center of mass visuals
    /// are currently visible
    public: std::map<Entity, bool> viewingCOM;

    /// \brief A map of links and their center of mass visuals
    public: std::map<Entity, Entity> linkToCOMVisuals;

    /// \brief Target to view center of mass
    public: std::string viewCOMTarget;

    /// \brief View center of mass service
    public: std::string viewCOMService;

    /////////////////////////////////////////////////
    // Inertia
    /////////////////////////////////////////////////

    /// \brief View inertia service
    public: std::string viewInertiaService;

    /// \brief A map of entity ids and their inertials
    public: std::map<Entity, math::Inertiald> entityInertials;

    /// \brief A map of links and their inertia visuals
    public: std::map<Entity, Entity> linkToInertiaVisuals;

    /// \brief A map of link entities and if their inertias are currently
    /// visible
    public: std::map<Entity, bool> viewingInertias;

    /// \brief A list of links used to create new inertia visuals
    public: std::vector<Entity> newInertiaLinks;

    /// \brief New inertias to be created
    public: std::vector<Entity> newInertias;

    /// \brief Target to view inertia
    public: std::string viewInertiaTarget;

    /////////////////////////////////////////////////
    // Collision
    /////////////////////////////////////////////////

    /// \brief Target to view collisions
    public: std::string viewCollisionsTarget;

    /// \brief View collisions service
    public: std::string viewCollisionsService;

    /// \brief New collisions to be created
    public: std::vector<Entity> newCollisions;

    /// \brief A list of links used to create new collision visuals
    public: std::vector<Entity> newCollisionLinks;

    /// \brief A map of collision entity ids and their SDF DOM
    public: std::map<Entity, sdf::Collision> entityCollisions;

    /// \brief A map of link entities and their corresponding children
    /// collisions
    public: std::map<Entity, std::vector<Entity>> linkToCollisionEntities;

    /// \brief A map of created collision entities and if they are currently
    /// visible
    public: std::map<Entity, bool> viewingCollisions;
  };
}

using namespace ignition;
using namespace gazebo;

/////////////////////////////////////////////////
void VisualizationCapabilitiesPrivate::OnRender()
{
  if (nullptr == this->scene)
  {
    this->scene = rendering::sceneFromFirstRenderEngine();
    if (nullptr == this->scene)
    {
      return;
    }
    this->sceneManager.SetScene(this->scene);
  }

  // create new wireframe visuals
  for (const auto &link : this->newWireframeVisualLinks)
  {
    std::vector<Entity> visEntities =
        this->linkToVisualEntities[link];

    for (const auto &visEntity : visEntities)
    {
      if (!this->viewingWireframes[visEntity])
      {
        auto wireframeVisual = this->VisualById(visEntity);
        if (wireframeVisual)
        {
          wireframeVisual->SetWireframe(true);
          this->viewingWireframes[visEntity] = true;
        }
      }
    }
  }
  this->newWireframeVisualLinks.clear();

  // create new transparent visuals
  for (const auto &link : this->newTransparentVisualLinks)
  {
    std::vector<Entity> visEntities =
        this->linkToVisualEntities[link];

    for (const auto &visEntity : visEntities)
    {
      if (!this->viewingTransparent[visEntity])
      {
        auto transparencyVisual = this->VisualById(visEntity);
        if (transparencyVisual)
        {
          this->sceneManager.UpdateTransparency(transparencyVisual,
              true /* transparent */);
          this->viewingTransparent[visEntity] = true;
        }
      }
    }
  }
  this->newTransparentVisualLinks.clear();

  // create new inertia visuals
  for (const auto &link : this->newInertiaLinks)
  {
    // create a new id for the inertia visual
    auto attempts = 100000u;
    for (auto i = 0u; i < attempts; ++i)
    {
      Entity id = i;
      if (!this->scene->HasNodeId(id) && !this->scene->HasLightId(id) &&
          !this->scene->HasSensorId(id) && !this->scene->HasVisualId(id) &&
          !this->viewingInertias[link])
      {
        auto existsVisual = this->VisualById(id);
        auto parentInertiaVisual = this->VisualById(link);

        if (existsVisual == nullptr && parentInertiaVisual != nullptr)
        {
          this->CreateInertiaVisual(id, this->entityInertials[link],
            parentInertiaVisual);
        }
        else
        {
          continue;
        }
        this->viewingInertias[link] = true;
        this->linkToInertiaVisuals[link] = id;
        break;
      }
    }
  }
  this->newInertiaLinks.clear();

  // create new center of mass visuals
  for (const auto &link : this->newCOMLinks)
  {
    // create a new id for the center of mass visual
    auto attempts = 100000u;
    for (auto i = 0u; i < attempts; ++i)
    {
      Entity id = i;
      if (!this->scene->HasNodeId(id) && !this->scene->HasLightId(id) &&
          !this->scene->HasSensorId(id) && !this->scene->HasVisualId(id) &&
          !this->viewingCOM[link])
      {
        auto existsVisual = this->VisualById(id);
        auto parentInertiaVisual = this->VisualById(link);

        if (existsVisual == nullptr && parentInertiaVisual != nullptr)
        {
          this->createCOMVisual(id, this->entityInertials[link],
            parentInertiaVisual);
        }
        else
        {
          continue;
        }
        this->viewingCOM[link] = true;
        this->linkToCOMVisuals[link] = id;
        break;
      }
    }
  }
  this->newCOMLinks.clear();

  // create new collision visuals
  for (const auto &link : this->newCollisionLinks)
  {
    std::vector<Entity> colEntities =
        this->linkToCollisionEntities[link];

    for (const auto &colEntity : colEntities)
    {
      if (!this->scene->HasNodeId(colEntity) &&
          !this->scene->HasLightId(colEntity) &&
          !this->scene->HasSensorId(colEntity) &&
          !this->scene->HasVisualId(colEntity) &&
          !this->viewingCollisions[link])
      {
        auto parentCollisionVisual = this->VisualById(link);
        if (parentCollisionVisual != nullptr)
        {
          auto vis = this->CreateCollision(
            colEntity,
            this->entityCollisions[colEntity],
            parentCollisionVisual);

          if (vis == nullptr)
          {
            continue;
          }

          this->viewingCollisions[colEntity] = true;

          // add geometry material to originalEmissive map
          for (auto g = 0u; g < vis->GeometryCount(); ++g)
          {
            auto geom = vis->GeometryByIndex(g);

            // Geometry material
            auto geomMat = geom->Material();
            if (nullptr == geomMat)
              continue;
          }
        }
        else
        {
          continue;
        }
      }
    }
  }
  this->newCollisionLinks.clear();

  // View center of mass
  {
    IGN_PROFILE("IgnRenderer::Render ViewCOM");
    if (!this->viewCOMTarget.empty())
    {
      rendering::NodePtr targetNode =
          scene->NodeByName(this->viewCOMTarget);
      auto targetVis = std::dynamic_pointer_cast<rendering::Visual>(targetNode);

      if (targetVis)
      {
        Entity targetEntity =
            std::get<int>(targetVis->UserData("gazebo-entity"));
        this->ViewCOM(targetEntity);
      }
      else
      {
        ignerr << "Unable to find node name ["
               << this->viewCOMTarget
               << "] to view center of mass" << std::endl;
      }

      this->viewCOMTarget.clear();
    }
  }

  // View inertia
  {
    IGN_PROFILE("IgnRenderer::Render ViewInertia");
    if (!this->viewInertiaTarget.empty())
    {
      rendering::NodePtr targetNode =
          scene->NodeByName(this->viewInertiaTarget);
      auto targetVis = std::dynamic_pointer_cast<rendering::Visual>(targetNode);

      if (targetVis)
      {
        Entity targetEntity =
            std::get<int>(targetVis->UserData("gazebo-entity"));
        this->ViewInertia(targetEntity);
      }
      else
      {
        ignerr << "Unable to find node name ["
               << this->viewInertiaTarget
               << "] to view inertia" << std::endl;
      }

      this->viewInertiaTarget.clear();
    }
  }

  // view Transparent
  {
    IGN_PROFILE("IgnRenderer::Render ViewTransparent");
    if (!this->viewTransparentTarget.empty())
    {
      rendering::NodePtr targetNode =
          this->scene->VisualByName(this->viewTransparentTarget);
      auto targetVis = std::dynamic_pointer_cast<rendering::Visual>(targetNode);

      if (targetVis)
      {
        Entity targetEntity =
            std::get<int>(targetVis->UserData("gazebo-entity"));
        this->ViewTransparent(targetEntity);
      }
      else
      {
        ignerr << "Unable to find node name ["
               << this->viewTransparentTarget
               << "] to view as transparent" << std::endl;
      }

      this->viewTransparentTarget.clear();
    }
  }

  // View collisions
  {
    IGN_PROFILE("IgnRenderer::Render ViewCollisions");
    if (!this->viewCollisionsTarget.empty())
    {
      rendering::NodePtr targetNode =
          scene->NodeByName(this->viewCollisionsTarget);
      auto targetVis = std::dynamic_pointer_cast<rendering::Visual>(targetNode);

      if (targetVis)
      {
        Entity targetEntity =
            std::get<int>(targetVis->UserData("gazebo-entity"));
        this->ViewCollisions(targetEntity);
      }
      else
      {
        ignerr << "Unable to find node name ["
               << this->viewCollisionsTarget
               << "] to view collisions" << std::endl;
      }

      this->viewCollisionsTarget.clear();
    }
  }

  // View wireframes
  {
    IGN_PROFILE("IgnRenderer::Render ViewWireframes");
    if (!this->viewWireframesTarget.empty())
    {
      rendering::NodePtr targetNode =
          scene->NodeByName(this->viewWireframesTarget);
      auto targetVis = std::dynamic_pointer_cast<rendering::Visual>(targetNode);

      if (targetVis)
      {
        Entity targetEntity =
            std::get<int>(targetVis->UserData("gazebo-entity"));
        this->ViewWireframes(targetEntity);
      }
      else
      {
        ignerr << "Unable to find node name ["
               << this->viewWireframesTarget
               << "] to view wireframes" << std::endl;
      }

      this->viewWireframesTarget.clear();
    }
  }
}

/////////////////////////////////////////////////
rendering::VisualPtr VisualizationCapabilitiesPrivate::CreateInertiaVisual(
  ignition::gazebo::Entity _id,
  const math::Inertiald &_inertia,
  ignition::rendering::VisualPtr &_parent)
{
  std::string name = "Inertia_" + std::to_string(_id);
  if (_parent)
    name = _parent->Name() + "::" + name;

  rendering::InertiaVisualPtr inertiaVisual =
    this->scene->CreateInertiaVisual(name);
  inertiaVisual->SetInertial(_inertia);

  rendering::VisualPtr inertiaVis =
    std::dynamic_pointer_cast<rendering::Visual>(inertiaVisual);
  inertiaVis->SetUserData("gazebo-entity", static_cast<int>(_id));
  inertiaVis->SetUserData("pause-update", static_cast<int>(0));
  this->visuals[_id] = inertiaVis;
  if (_parent)
  {
    inertiaVis->RemoveParent();
    _parent->AddChild(inertiaVis);
  }
  return inertiaVis;
}

/////////////////////////////////////////////////
rendering::VisualPtr VisualizationCapabilitiesPrivate::CreateCollision(
  ignition::gazebo::Entity _id,
  const sdf::Collision &_collision,
  rendering::VisualPtr &_parent)
{
  sdf::Material material;
  material.SetAmbient(math::Color(1.0f, 0.5088f, 0.0468f, 0.7f));
  material.SetDiffuse(math::Color(1.0f, 0.5088f, 0.0468f, 0.7f));

  sdf::Visual visual;
  visual.SetGeom(*_collision.Geom());
  visual.SetMaterial(material);
  visual.SetCastShadows(false);

  visual.SetRawPose(_collision.RawPose());
  visual.SetName(_collision.Name());

  rendering::VisualPtr collisionVis = CreateVisual(_id, visual, _parent);
  return collisionVis;
}

/////////////////////////////////////////////////
rendering::GeometryPtr VisualizationCapabilitiesPrivate::LoadGeometry(
  const sdf::Geometry &_geom, math::Vector3d &_scale,
  math::Pose3d &_localPose)
{
  if (!this->scene)
    return rendering::GeometryPtr();

  math::Vector3d scale = math::Vector3d::One;
  math::Pose3d localPose = math::Pose3d::Zero;
  rendering::GeometryPtr geom{nullptr};
  if (_geom.Type() == sdf::GeometryType::BOX)
  {
    geom = this->scene->CreateBox();
    scale = _geom.BoxShape()->Size();
  }
  else if (_geom.Type() == sdf::GeometryType::CAPSULE)
  {
    auto capsule = this->scene->CreateCapsule();
    capsule->SetRadius(_geom.CapsuleShape()->Radius());
    capsule->SetLength(_geom.CapsuleShape()->Length());
    geom = capsule;
  }
  else if (_geom.Type() == sdf::GeometryType::CYLINDER)
  {
    geom = this->scene->CreateCylinder();
    scale.X() = _geom.CylinderShape()->Radius() * 2;
    scale.Y() = scale.X();
    scale.Z() = _geom.CylinderShape()->Length();
  }
  else if (_geom.Type() == sdf::GeometryType::ELLIPSOID)
  {
    geom = this->scene->CreateSphere();
    scale.X() = _geom.EllipsoidShape()->Radii().X() * 2;
    scale.Y() = _geom.EllipsoidShape()->Radii().Y() * 2;
    scale.Z() = _geom.EllipsoidShape()->Radii().Z() * 2;
  }
  else if (_geom.Type() == sdf::GeometryType::PLANE)
  {
    geom = this->scene->CreatePlane();
    if (geom == nullptr)
      return rendering::GeometryPtr();
    scale.X() = _geom.PlaneShape()->Size().X();
    scale.Y() = _geom.PlaneShape()->Size().Y();

    // Create a rotation for the plane mesh to account for the normal vector.
    // The rotation is the angle between the +z(0,0,1) vector and the
    // normal, which are both expressed in the local (Visual) frame.
    math::Vector3d normal = _geom.PlaneShape()->Normal();
    localPose.Rot().From2Axes(math::Vector3d::UnitZ, normal.Normalized());
  }
  else if (_geom.Type() == sdf::GeometryType::SPHERE)
  {
    geom = this->scene->CreateSphere();
    scale.X() = _geom.SphereShape()->Radius() * 2;
    scale.Y() = scale.X();
    scale.Z() = scale.X();
  }
  else if (_geom.Type() == sdf::GeometryType::MESH)
  {
    auto fullPath = asFullPath(_geom.MeshShape()->Uri(),
        _geom.MeshShape()->FilePath());
    if (fullPath.empty())
    {
      ignerr << "Mesh geometry missing uri" << std::endl;
      return geom;
    }
    rendering::MeshDescriptor descriptor;

    // Assume absolute path to mesh file
    descriptor.meshName = fullPath;
    descriptor.subMeshName = _geom.MeshShape()->Submesh();
    descriptor.centerSubMesh = _geom.MeshShape()->CenterSubmesh();

    ignition::common::MeshManager *meshManager =
        ignition::common::MeshManager::Instance();
    descriptor.mesh = meshManager->Load(descriptor.meshName);
    geom = this->scene->CreateMesh(descriptor);
    scale = _geom.MeshShape()->Scale();
  }
  else if (_geom.Type() == sdf::GeometryType::HEIGHTMAP)
  {
    auto fullPath = asFullPath(_geom.HeightmapShape()->Uri(),
        _geom.HeightmapShape()->FilePath());
    if (fullPath.empty())
    {
      ignerr << "Heightmap geometry missing URI" << std::endl;
      return geom;
    }

    auto data = std::make_shared<common::ImageHeightmap>();
    if (data->Load(fullPath) < 0)
    {
      ignerr << "Failed to load heightmap image data from [" << fullPath << "]"
             << std::endl;
      return geom;
    }

    rendering::HeightmapDescriptor descriptor;
    descriptor.SetData(data);
    descriptor.SetSize(_geom.HeightmapShape()->Size());
    descriptor.SetSampling(_geom.HeightmapShape()->Sampling());

    for (uint64_t i = 0; i < _geom.HeightmapShape()->TextureCount(); ++i)
    {
      auto textureSdf = _geom.HeightmapShape()->TextureByIndex(i);
      rendering::HeightmapTexture textureDesc;
      textureDesc.SetSize(textureSdf->Size());
      textureDesc.SetDiffuse(asFullPath(textureSdf->Diffuse(),
          _geom.HeightmapShape()->FilePath()));
      textureDesc.SetNormal(asFullPath(textureSdf->Normal(),
          _geom.HeightmapShape()->FilePath()));
      descriptor.AddTexture(textureDesc);
    }

    for (uint64_t i = 0; i < _geom.HeightmapShape()->BlendCount(); ++i)
    {
      auto blendSdf = _geom.HeightmapShape()->BlendByIndex(i);
      rendering::HeightmapBlend blendDesc;
      blendDesc.SetMinHeight(blendSdf->MinHeight());
      blendDesc.SetFadeDistance(blendSdf->FadeDistance());
      descriptor.AddBlend(blendDesc);
    }

    geom = this->scene->CreateHeightmap(descriptor);
    if (nullptr == geom)
    {
      ignerr << "Failed to create heightmap [" << fullPath << "]" << std::endl;
    }
    scale = _geom.HeightmapShape()->Size();
  }
  else
  {
    ignerr << "Unsupported geometry type" << std::endl;
  }
  _scale = scale;
  _localPose = localPose;
  return geom;
}

/////////////////////////////////////////////////
rendering::MaterialPtr VisualizationCapabilitiesPrivate::LoadMaterial(
    const sdf::Material &_material)
{
  if (!this->scene)
    return rendering::MaterialPtr();

  rendering::MaterialPtr material = this->scene->CreateMaterial();
  material->SetAmbient(_material.Ambient());
  material->SetDiffuse(_material.Diffuse());
  material->SetSpecular(_material.Specular());
  material->SetEmissive(_material.Emissive());
  material->SetRenderOrder(_material.RenderOrder());

  // parse PBR params
  const sdf::Pbr *pbr = _material.PbrMaterial();
  if (pbr)
  {
    sdf::PbrWorkflow *workflow = nullptr;
    const sdf::PbrWorkflow *metal =
        pbr->Workflow(sdf::PbrWorkflowType::METAL);
    if (metal)
    {
      double roughness = metal->Roughness();
      double metalness = metal->Metalness();
      material->SetRoughness(roughness);
      material->SetMetalness(metalness);

      // roughness map
      std::string roughnessMap = metal->RoughnessMap();
      if (!roughnessMap.empty())
      {
        std::string fullPath = common::findFile(
            asFullPath(roughnessMap, _material.FilePath()));
        if (!fullPath.empty())
          material->SetRoughnessMap(fullPath);
        else
          ignerr << "Unable to find file [" << roughnessMap << "]\n";
      }

      // metalness map
      std::string metalnessMap = metal->MetalnessMap();
      if (!metalnessMap.empty())
      {
        std::string fullPath = common::findFile(
            asFullPath(metalnessMap, _material.FilePath()));
        if (!fullPath.empty())
          material->SetMetalnessMap(fullPath);
        else
          ignerr << "Unable to find file [" << metalnessMap << "]\n";
      }
      workflow = const_cast<sdf::PbrWorkflow *>(metal);
    }
    else
    {
      ignerr << "PBR material: currently only metal workflow is supported"
             << std::endl;
    }

    // albedo map
    std::string albedoMap = workflow->AlbedoMap();
    if (!albedoMap.empty())
    {
      std::string fullPath = common::findFile(
          asFullPath(albedoMap, _material.FilePath()));
      if (!fullPath.empty())
      {
        material->SetTexture(fullPath);
        // Use alpha channel for transparency
        material->SetAlphaFromTexture(true, 0.5, _material.DoubleSided());
      }
      else
        ignerr << "Unable to find file [" << albedoMap << "]\n";
    }

    // normal map
    std::string normalMap = workflow->NormalMap();
    if (!normalMap.empty())
    {
      std::string fullPath = common::findFile(
          asFullPath(normalMap, _material.FilePath()));
      if (!fullPath.empty())
        material->SetNormalMap(fullPath);
      else
        ignerr << "Unable to find file [" << normalMap << "]\n";
    }


    // environment map
    std::string environmentMap = workflow->EnvironmentMap();
    if (!environmentMap.empty())
    {
      std::string fullPath = common::findFile(
          asFullPath(environmentMap, _material.FilePath()));
      if (!fullPath.empty())
        material->SetEnvironmentMap(fullPath);
      else
        ignerr << "Unable to find file [" << environmentMap << "]\n";
    }

    // emissive map
    std::string emissiveMap = workflow->EmissiveMap();
    if (!emissiveMap.empty())
    {
      std::string fullPath = common::findFile(
          asFullPath(emissiveMap, _material.FilePath()));
      if (!fullPath.empty())
        material->SetEmissiveMap(fullPath);
      else
        ignerr << "Unable to find file [" << emissiveMap << "]\n";
    }

    // light map
    std::string lightMap = workflow->LightMap();
    if (!lightMap.empty())
    {
      std::string fullPath = common::findFile(
          asFullPath(lightMap, _material.FilePath()));
      if (!fullPath.empty())
      {
        unsigned int uvSet = workflow->LightMapTexCoordSet();
        material->SetLightMap(fullPath, uvSet);
      }
      else
      {
        ignerr << "Unable to find file [" << lightMap << "]\n";
      }
    }
  }
  return material;
}

/////////////////////////////////////////////////
rendering::VisualPtr VisualizationCapabilitiesPrivate::CreateVisual(
  ignition::gazebo::Entity _id,
  const sdf::Visual &_visual,
  rendering::VisualPtr &_parent)
{
  if (!this->scene)
    return rendering::VisualPtr();

  if (this->visuals.find(_id) != this->visuals.end())
  {
    return rendering::VisualPtr();
  }

  if (!_visual.Geom())
    return rendering::VisualPtr();

  std::string name = _visual.Name().empty() ? std::to_string(_id) :
      _visual.Name();
  if (_parent)
    name = _parent->Name() + "::" + name;
  if (this->scene->HasVisualName(name))
  {
    auto vis = this->scene->VisualByName(name);
    this->visuals[_id] = vis;
    return vis;
  }
  rendering::VisualPtr visualVis = this->scene->CreateVisual(name);
  visualVis->SetUserData("gazebo-entity", static_cast<int>(_id));
  visualVis->SetUserData("pause-update", static_cast<int>(0));
  visualVis->SetLocalPose(_visual.RawPose());

  math::Vector3d scale = math::Vector3d::One;
  math::Pose3d localPose;
  rendering::GeometryPtr geom =
      this->LoadGeometry(*_visual.Geom(), scale, localPose);

  if (geom)
  {
    /// localPose is currently used to handle the normal vector in plane visuals
    /// In general, this can be used to store any local transforms between the
    /// parent Visual and geometry.
    if (localPose != math::Pose3d::Zero)
    {
      rendering::VisualPtr geomVis =
          this->scene->CreateVisual(name + "_geom");
      geomVis->AddGeometry(geom);
      geomVis->SetLocalPose(localPose);
      visualVis->AddChild(geomVis);
    }
    else
    {
      visualVis->AddGeometry(geom);
    }

    visualVis->SetLocalScale(scale);

    // set material
    rendering::MaterialPtr material{nullptr};
    if (_visual.Geom()->Type() == sdf::GeometryType::HEIGHTMAP)
    {
      // Heightmap's material is loaded together with it.
    }
    else if (_visual.Material())
    {
      material = this->LoadMaterial(*_visual.Material());
    }
    // Don't set a default material for meshes because they
    // may have their own
    // TODO(anyone) support overriding mesh material
    else if (_visual.Geom()->Type() != sdf::GeometryType::MESH)
    {
      // create default material
      material = this->scene->Material("ign-grey");
      if (!material)
      {
        material = this->scene->CreateMaterial("ign-grey");
        material->SetAmbient(0.3, 0.3, 0.3);
        material->SetDiffuse(0.7, 0.7, 0.7);
        material->SetSpecular(1.0, 1.0, 1.0);
        material->SetRoughness(0.2f);
        material->SetMetalness(1.0f);
      }
    }
    else
    {
      // meshes created by mesh loader may have their own materials
      // update/override their properties based on input sdf element values
      auto mesh = std::dynamic_pointer_cast<rendering::Mesh>(geom);
      for (unsigned int i = 0; i < mesh->SubMeshCount(); ++i)
      {
        auto submesh = mesh->SubMeshByIndex(i);
        auto submeshMat = submesh->Material();
        if (submeshMat)
        {
          double productAlpha = (1.0-_visual.Transparency()) *
              (1.0 - submeshMat->Transparency());
          submeshMat->SetTransparency(1 - productAlpha);
          submeshMat->SetCastShadows(_visual.CastShadows());
        }
      }
    }

    if (material)
    {
      // set transparency
      material->SetTransparency(_visual.Transparency());

      // cast shadows
      material->SetCastShadows(_visual.CastShadows());

      geom->SetMaterial(material);
      // todo(anyone) SetMaterial function clones the input material.
      // but does not take ownership of it so we need to destroy it here.
      // This is not ideal. We should let ign-rendering handle the lifetime
      // of this material
      this->scene->DestroyMaterial(material);
    }
  }
  else
  {
    ignerr << "Failed to load geometry for visual: " << _visual.Name()
           << std::endl;
  }

  // visibility flags
  visualVis->SetVisibilityFlags(_visual.VisibilityFlags());

  this->visuals[_id] = visualVis;
  if (_parent)
    _parent->AddChild(visualVis);

  return visualVis;

}

/////////////////////////////////////////////////
rendering::VisualPtr VisualizationCapabilitiesPrivate::createCOMVisual(
  ignition::gazebo::Entity _id,
  const math::Inertiald &_inertia,
  rendering::VisualPtr &_parent)
{
  std::string name = "COM_" + std::to_string(_id);
  if (_parent)
    name = _parent->Name() + "::" + name;

  rendering::COMVisualPtr comVisual =
    this->scene->CreateCOMVisual(name);
  comVisual->SetInertial(_inertia);

  rendering::VisualPtr comVis =
    std::dynamic_pointer_cast<rendering::Visual>(comVisual);
  comVis->SetUserData("gazebo-entity", static_cast<int>(_id));
  comVis->SetUserData("pause-update", static_cast<int>(0));
  this->visuals[_id] = comVis;

  if (_parent)
  {
    comVis->RemoveParent();
    _parent->AddChild(comVis);
  }

  return comVis;
}

ignition::rendering::VisualPtr VisualizationCapabilitiesPrivate::VisualById(
  unsigned int _id)
{
  for (unsigned int i = 0; i < this->scene->VisualCount(); ++i)
  {
    auto visual = this->scene->VisualByIndex(i);

    try {
      Entity visualEntity =
          std::get<int>(visual->UserData("gazebo-entity"));

      if (visualEntity == _id)
      {
        return visual;
      }
    }
    catch (std::bad_variant_access &)
    {
      // It's ok to get here
    }
  }
  return nullptr;
}

/////////////////////////////////////////////////
bool VisualizationCapabilitiesPrivate::OnViewTransparent(
  const msgs::StringMsg &_msg, msgs::Boolean &_res)
{
  this->viewTransparentTarget = _msg.data();

  _res.set_data(true);
  return true;
}

/////////////////////////////////////////////////
bool VisualizationCapabilitiesPrivate::OnViewWireframes(
  const msgs::StringMsg &_msg, msgs::Boolean &_res)
{
  this->viewWireframesTarget = _msg.data();

  _res.set_data(true);
  return true;
}

/////////////////////////////////////////////////
bool VisualizationCapabilitiesPrivate::OnViewCOM(
  const msgs::StringMsg &_msg, msgs::Boolean &_res)
{
  this->viewCOMTarget = _msg.data();

  _res.set_data(true);
  return true;
}

/////////////////////////////////////////////////
bool VisualizationCapabilitiesPrivate::OnViewInertia(
  const msgs::StringMsg &_msg, msgs::Boolean &_res)
{
  this->viewInertiaTarget = _msg.data();

  _res.set_data(true);
  return true;
}

/////////////////////////////////////////////////
bool VisualizationCapabilitiesPrivate::OnViewCollisions(
  const msgs::StringMsg &_msg, msgs::Boolean &_res)
{
  this->viewCollisionsTarget = _msg.data();

  _res.set_data(true);
  return true;
}

/////////////////////////////////////////////////
void VisualizationCapabilitiesPrivate::ViewCollisions(const Entity &_entity)
{
  std::vector<Entity> colEntities;

  if (this->linkToCollisionEntities.find(_entity) !=
      this->linkToCollisionEntities.end())
  {
    colEntities = this->linkToCollisionEntities[_entity];
  }

  // Find all existing child links for this entity
  std::vector<Entity> links = std::move(this->FindChildLinks(_entity));

  for (const auto &link : links)
  {
    colEntities.insert(colEntities.end(),
        this->linkToCollisionEntities[link].begin(),
        this->linkToCollisionEntities[link].end());
  }

  // create and/or toggle collision visuals
  bool showCol, showColInit = false;

  // first loop looks for new collisions
  for (const auto &colEntity : colEntities)
  {
    if (this->viewingCollisions.find(colEntity) ==
        this->viewingCollisions.end())
    {
      this->newCollisions.push_back(_entity);
      showColInit = showCol = true;
    }
  }

  // second loop toggles already created collisions
  for (const auto &colEntity : colEntities)
  {
    if (this->viewingCollisions.find(colEntity) ==
        this->viewingCollisions.end())
      continue;

    // when viewing multiple collisions (e.g. _entity is a model),
    // boolean for view collisions is based on first colEntity in list
    if (!showColInit)
    {
      showCol = !this->viewingCollisions[colEntity];
      showColInit = true;
    }

    auto colVisual = this->VisualById(colEntity);
    if (colVisual)
    {
      this->viewingCollisions[colEntity] = showCol;
      colVisual->SetVisible(showCol);
    }
  }
}

/////////////////////////////////////////////////
void VisualizationCapabilitiesPrivate::ViewInertia(const Entity &_entity)
{
  std::vector<Entity> inertiaLinks = std::move(this->FindChildLinks(_entity));

  // check if _entity has an inertial component (_entity is a link)
  if (this->entityInertials.find(_entity) !=
      this->entityInertials.end())
    inertiaLinks.push_back(_entity);

  // create and/or toggle inertia visuals
  bool showInertia, showInertiaInit = false;
  // first loop looks for new inertias
  for (const auto &inertiaLink : inertiaLinks)
  {
    if (this->viewingInertias.find(inertiaLink) ==
        this->viewingInertias.end())
    {
      this->newInertias.push_back(_entity);
      showInertiaInit = showInertia = true;
    }
  }

  // second loop toggles already created inertias
  for (const auto &inertiaLink : inertiaLinks)
  {
    if (this->viewingInertias.find(inertiaLink) ==
        this->viewingInertias.end())
      continue;

    // when viewing multiple inertias (e.g. _entity is a model),
    // boolean for view inertias is based on first inrEntity in list
    if (!showInertiaInit)
    {
      showInertia = !this->viewingInertias[inertiaLink];
      showInertiaInit = true;
    }

    Entity inertiaVisualId = this->linkToInertiaVisuals[inertiaLink];

    auto inertiaVisual = this->VisualById(inertiaVisualId);
    if (inertiaVisual)
    {
      this->viewingInertias[inertiaLink] = showInertia;
      inertiaVisual->SetVisible(showInertia);
    }
  }
}

/////////////////////////////////////////////////
void VisualizationCapabilitiesPrivate::ViewCOM(const Entity &_entity)
{
  std::vector<Entity> inertiaLinks = std::move(this->FindChildLinks(_entity));

  // check if _entity has an inertial component (_entity is a link)
  if (this->entityInertials.find(_entity) !=
      this->entityInertials.end())
    inertiaLinks.push_back(_entity);

  // create and/or toggle center of mass visuals
  bool showCOM, showCOMInit = false;
  // first loop looks for new center of mass visuals
  for (const auto &inertiaLink : inertiaLinks)
  {
    if (this->viewingCOM.find(inertiaLink) ==
        this->viewingCOM.end())
    {
      this->newCOMVisuals.push_back(_entity);
      showCOMInit = showCOM = true;
    }
  }

  // second loop toggles already created center of mass visuals
  for (const auto &inertiaLink : inertiaLinks)
  {
    if (this->viewingCOM.find(inertiaLink) ==
        this->viewingCOM.end())
      continue;

    // when viewing multiple center of mass visuals (e.g. _entity is a model),
    // boolean for view center of mass is based on first inertiaEntity in list
    if (!showCOMInit)
    {
      showCOM = !this->viewingCOM[inertiaLink];
      showCOMInit = true;
    }

    Entity comVisualId = this->linkToCOMVisuals[inertiaLink];

    auto comVisual = this->VisualById(comVisualId);
    if (comVisual)
    {
      this->viewingCOM[inertiaLink] = showCOM;
      comVisual->SetVisible(showCOM);
    }
  }
}

/////////////////////////////////////////////////
void VisualizationCapabilitiesPrivate::ViewWireframes(const Entity &_entity)
{
  std::vector<Entity> visEntities;

  if (this->linkToVisualEntities.find(_entity) !=
      this->linkToVisualEntities.end())
  {
    visEntities = this->linkToVisualEntities[_entity];
  }

  // Find all existing child links for this entity
  std::vector<Entity> links = std::move(this->FindChildLinks(_entity));

  for (const auto &link : links)
  {
    visEntities.insert(visEntities.end(),
        this->linkToVisualEntities[link].begin(),
        this->linkToVisualEntities[link].end());
  }

  // Toggle wireframes
  bool showWireframe, showWireframeInit = false;

  // first loop looks for new wireframes
  for (const auto &visEntity : visEntities)
  {
    if (this->viewingWireframes.find(visEntity) ==
        this->viewingWireframes.end())
    {
      this->newWireframes.push_back(_entity);
      showWireframeInit = showWireframe = true;
    }
  }

  // second loop toggles wireframes
  for (const auto &visEntity : visEntities)
  {
    if (this->viewingWireframes.find(visEntity) ==
        this->viewingWireframes.end())
      continue;

    // when viewing multiple wireframes (e.g. _entity is a model),
    // boolean for view wireframe is based on first visEntity in list
    if (!showWireframeInit)
    {
      showWireframe = !this->viewingWireframes[visEntity];
      showWireframeInit = true;
    }

    auto wireframesVisual = this->VisualById(visEntity);
    if (wireframesVisual)
    {
      this->viewingWireframes[visEntity] = showWireframe;
      wireframesVisual->SetWireframe(showWireframe);
    }
  }
}

/////////////////////////////////////////////////
void VisualizationCapabilitiesPrivate::ViewTransparent(const Entity &_entity)
{
  std::vector<Entity> visEntities;

  if (this->linkToVisualEntities.find(_entity) !=
      this->linkToVisualEntities.end())
  {
    visEntities = this->linkToVisualEntities[_entity];
  }

  // Find all existing child links for this entity
  std::vector<Entity> links = std::move(this->FindChildLinks(_entity));

  for (const auto &link : links)
  {
    visEntities.insert(visEntities.end(),
        this->linkToVisualEntities[link].begin(),
        this->linkToVisualEntities[link].end());
  }

  // Toggle transparent mode
  bool showTransparent, showTransparentInit = false;

  // first loop looks for new transparent entities
  for (const auto &visEntity : visEntities)
  {
    if (this->viewingTransparent.find(visEntity) ==
        this->viewingTransparent.end())
    {
      this->newTransparentEntities.push_back(_entity);
      showTransparentInit = showTransparent = true;
    }
  }

  // second loop toggles transparent mode
  for (const auto &visEntity : visEntities)
  {
    if (this->viewingTransparent.find(visEntity) ==
        this->viewingTransparent.end())
      continue;

    // when viewing multiple transparent visuals (e.g. _entity is a model),
    // boolean for view as transparent is based on first visEntity in list
    if (!showTransparentInit)
    {
      showTransparent = !this->viewingTransparent[visEntity];
      showTransparentInit = true;
    }

    auto transparentVisual = this->VisualById(visEntity);
    if (transparentVisual)
    {
      this->viewingTransparent[visEntity] = showTransparent;

      this->sceneManager.UpdateTransparency(transparentVisual,
                showTransparent);
    }
  }
}

/////////////////////////////////////////////////
std::vector<Entity> VisualizationCapabilitiesPrivate::FindChildLinks(
  const Entity &_entity)
{
  std::vector<Entity> links;

  if (this->modelToLinkEntities.find(_entity) !=
           this->modelToLinkEntities.end())
  {
    links.insert(links.end(),
        this->modelToLinkEntities[_entity].begin(),
        this->modelToLinkEntities[_entity].end());
  }

  if (this->modelToModelEntities.find(_entity) !=
      this->modelToModelEntities.end())
  {
    std::stack<Entity> modelStack;
    modelStack.push(_entity);

    std::vector<Entity> childModels;
    while (!modelStack.empty())
    {
      Entity model = modelStack.top();
      modelStack.pop();

      links.insert(links.end(),
          this->modelToLinkEntities[model].begin(),
          this->modelToLinkEntities[model].end());

      childModels = this->modelToModelEntities[model];
      for (const auto &childModel : childModels)
      {
        modelStack.push(childModel);
      }
    }
  }

  return links;
}

//////////////////////////////////////////////////
void VisualizationCapabilitiesPrivate::FindInertialLinks(
  const EntityComponentManager &_ecm)
{
  for (const auto &entity : this->newInertias)
  {
    std::vector<Entity> links;
    if (_ecm.EntityMatches(entity,
          std::set<ComponentTypeId>{components::Model::typeId}) ||
        _ecm.EntityMatches(entity,
                std::set<ComponentTypeId>{components::Link::typeId}))
    {
      links = std::move(this->FindChildLinksFromECM(_ecm, entity));
    }
    else
    {
      ignerr << "Entity [" << entity
             << "] for viewing inertia must be a model or link"
             << std::endl;
      continue;
    }

    this->newInertiaLinks.insert(this->newInertiaLinks.end(),
        links.begin(),
        links.end());
  }
  this->newInertias.clear();

  for (const auto &entity : this->newCOMVisuals)
  {
    std::vector<Entity> links;
    if (_ecm.EntityMatches(entity,
          std::set<ComponentTypeId>{components::Model::typeId}) ||
        _ecm.EntityMatches(entity,
                std::set<ComponentTypeId>{components::Link::typeId}))
    {
      links = std::move(this->FindChildLinksFromECM(_ecm, entity));
    }
    else
    {
      ignerr << "Entity [" << entity
             << "] for viewing center of mass must be a model or link"
             << std::endl;
      continue;
    }

    this->newCOMLinks.insert(this->newCOMLinks.end(),
        links.begin(),
        links.end());
  }
  this->newCOMVisuals.clear();
}

//////////////////////////////////////////////////
void VisualizationCapabilitiesPrivate::FindCollisionLinks(
  const EntityComponentManager &_ecm)
{
  if (this->newCollisions.empty())
    return;

  for (const auto &entity : this->newCollisions)
  {
    std::vector<Entity> links;
    if (_ecm.EntityMatches(entity,
          std::set<ComponentTypeId>{components::Model::typeId}) ||
        _ecm.EntityMatches(entity,
                std::set<ComponentTypeId>{components::Link::typeId}))
    {
      links = std::move(this->FindChildLinksFromECM(_ecm, entity));
    }
    else
    {
      ignerr << "Entity [" << entity
             << "] for viewing collision must be a model or link"
             << std::endl;
      continue;
    }

    this->newCollisionLinks.insert(this->newCollisionLinks.end(),
        links.begin(),
        links.end());
  }
  this->newCollisions.clear();
}

//////////////////////////////////////////////////
void VisualizationCapabilitiesPrivate::PopulateViewModeVisualLinks(
                        const EntityComponentManager &_ecm)
{
  // Find links to toggle wireframes
  for (const auto &entity : this->newWireframes)
  {
    std::vector<Entity> links;
    if (_ecm.EntityMatches(entity,
          std::set<ComponentTypeId>{components::Model::typeId}) ||
        _ecm.EntityMatches(entity,
                std::set<ComponentTypeId>{components::Link::typeId}))
    {
      links = std::move(this->FindChildLinksFromECM(_ecm, entity));
    }
    else
    {
      ignerr << "Entity [" << entity
             << "] for viewing wireframe must be a model or link"
             << std::endl;
      continue;
    }

    this->newWireframeVisualLinks.insert(this->newWireframeVisualLinks.end(),
        links.begin(),
        links.end());
  }
  this->newWireframes.clear();

  // Find links to view as transparent
  for (const auto &entity : this->newTransparentEntities)
  {
    std::vector<Entity> links;
    if (_ecm.EntityMatches(entity,
          std::set<ComponentTypeId>{components::Model::typeId}) ||
        _ecm.EntityMatches(entity,
                std::set<ComponentTypeId>{components::Link::typeId}))
    {
      links = std::move(this->FindChildLinksFromECM(_ecm, entity));
    }
    else
    {
      ignerr << "Entity [" << entity
             << "] for viewing as transparent must be a model or link"
             << std::endl;
      continue;
    }

    this->newTransparentVisualLinks.insert(
        this->newTransparentVisualLinks.end(),
        links.begin(),
        links.end());
  }
  this->newTransparentEntities.clear();
}

//////////////////////////////////////////////////
std::vector<Entity> VisualizationCapabilitiesPrivate::FindChildLinksFromECM(
    const EntityComponentManager &_ecm, const Entity &_entity)
{
  std::vector<Entity> links;
  if (_ecm.EntityMatches(_entity,
        std::set<ComponentTypeId>{components::Model::typeId}))
  {
    std::stack<Entity> modelStack;
    modelStack.push(_entity);

    std::vector<Entity> childLinks, childModels;
    while (!modelStack.empty())
    {
      Entity model = modelStack.top();
      modelStack.pop();

      childLinks = _ecm.EntitiesByComponents(components::ParentEntity(model),
                                             components::Link());
      links.insert(links.end(),
                   childLinks.begin(),
                   childLinks.end());

      childModels =
          _ecm.EntitiesByComponents(components::ParentEntity(model),
                                    components::Model());
      for (const auto &childModel : childModels)
      {
          modelStack.push(childModel);
      }
    }
  }
  else if (_ecm.EntityMatches(_entity,
              std::set<ComponentTypeId>{components::Link::typeId}))
  {
    links.push_back(_entity);
  }
  return links;
}

/////////////////////////////////////////////////
VisualizationCapabilities::VisualizationCapabilities()
  : GuiSystem(),
  dataPtr(std::make_unique<VisualizationCapabilitiesPrivate>())
{
}

/////////////////////////////////////////////////
VisualizationCapabilities::~VisualizationCapabilities() = default;

//////////////////////////////////////////////////
void VisualizationCapabilities::Update(const UpdateInfo &,
    EntityComponentManager &_ecm)
{
  if (!this->dataPtr->initialized)
  {
    _ecm.Each<components::Link, components::Name, components::Pose,
          components::ParentEntity>(
      [&](const Entity &_entity,
          const components::Link *,
          const components::Name *,
          const components::Pose *,
          const components::ParentEntity *_parent)->bool
      {
        this->dataPtr->modelToLinkEntities[_parent->Data()].push_back(_entity);
        return true;
      });

    // inertials
    _ecm.Each<components::Inertial, components::Pose>(
      [&](const Entity &_entity,
          const components::Inertial *_inrElement,
          const components::Pose *) -> bool
      {
        this->dataPtr->entityInertials[_entity] = _inrElement->Data();
        return true;
      });

    // visuals
    _ecm.Each<components::Visual, components::Name, components::Pose,
              components::Geometry,
              components::CastShadows,
              components::Transparency,
              components::VisibilityFlags,
              components::ParentEntity>(
        [&](const Entity &_entity,
            const components::Visual *,
            const components::Name *,
            const components::Pose *,
            const components::Geometry *,
            const components::CastShadows *,
            const components::Transparency *,
            const components::VisibilityFlags *,
            const components::ParentEntity *_parent)->bool
        {
          this->dataPtr->linkToVisualEntities[_parent->Data()]
            .push_back(_entity);
          return true;
        });

    _ecm.Each<components::Model, components::Name, components::Pose,
              components::ParentEntity>(
      [&](const Entity &_entity,
          const components::Model *,
          const components::Name *,
          const components::Pose *,
          const components::ParentEntity *_parent)->bool
      {
        this->dataPtr->modelToModelEntities[_parent->Data()].push_back(_entity);
        return true;
      });

    // collisions
    _ecm.Each<components::Collision, components::Name, components::Pose,
              components::Geometry, components::CollisionElement,
              components::ParentEntity>(
      [&](const Entity &_entity,
          const components::Collision *,
          const components::Name *,
          const components::Pose *,
          const components::Geometry *,
          const components::CollisionElement *_collElement,
          const components::ParentEntity *_parent) -> bool
      {
        this->dataPtr->entityCollisions[_entity] = _collElement->Data();
        this->dataPtr->linkToCollisionEntities[_parent->Data()]
          .push_back(_entity);
        return true;
      });
    this->dataPtr->initialized = true;
  }
  else
  {
    _ecm.EachNew<components::Link, components::Name, components::Pose,
          components::ParentEntity>(
      [&](const Entity &_entity,
          const components::Link *,
          const components::Name *,
          const components::Pose *,
          const components::ParentEntity *_parent)->bool
      {
        this->dataPtr->modelToLinkEntities[_parent->Data()].push_back(_entity);
        return true;
      });

    // inertials
    _ecm.EachNew<components::Inertial, components::Pose>(
      [&](const Entity &_entity,
          const components::Inertial *_inrElement,
          const components::Pose *) -> bool
      {
        this->dataPtr->entityInertials[_entity] = _inrElement->Data();
        return true;
      });

    // visuals
    _ecm.EachNew<components::Visual, components::Name, components::Pose,
              components::Geometry,
              components::CastShadows,
              components::Transparency,
              components::VisibilityFlags,
              components::ParentEntity>(
        [&](const Entity &_entity,
            const components::Visual *,
            const components::Name *,
            const components::Pose *,
            const components::Geometry *,
            const components::CastShadows *,
            const components::Transparency *,
            const components::VisibilityFlags *,
            const components::ParentEntity *_parent)->bool
        {
          this->dataPtr->linkToVisualEntities[_parent->Data()]
            .push_back(_entity);
          return true;
        });

    _ecm.EachNew<components::Model, components::Name, components::Pose,
              components::ParentEntity>(
      [&](const Entity &_entity,
          const components::Model *,
          const components::Name *,
          const components::Pose *,
          const components::ParentEntity *_parent)->bool
      {
        this->dataPtr->modelToModelEntities[_parent->Data()].push_back(_entity);
        return true;
      });

    // collisions
    _ecm.EachNew<components::Collision, components::Name, components::Pose,
              components::Geometry, components::CollisionElement,
              components::ParentEntity>(
      [&](const Entity &_entity,
          const components::Collision *,
          const components::Name *,
          const components::Pose *,
          const components::Geometry *,
          const components::CollisionElement *_collElement,
          const components::ParentEntity *_parent) -> bool
      {
        this->dataPtr->entityCollisions[_entity] = _collElement->Data();
        this->dataPtr->linkToCollisionEntities[_parent->Data()]
          .push_back(_entity);
        return true;
      });
  }

  _ecm.EachRemoved<components::Model>(
    [&](const Entity &_entity, const components::Model *)->bool
    {
      this->dataPtr->modelToLinkEntities.erase(_entity);
      this->dataPtr->modelToModelEntities.erase(_entity);
      return true;
    });

  _ecm.EachRemoved<components::Link>(
    [&](const Entity &_entity, const components::Link *)->bool
    {
      this->dataPtr->linkToVisualEntities.erase(_entity);
      return true;
    });

  this->dataPtr->PopulateViewModeVisualLinks(_ecm);
  this->dataPtr->FindInertialLinks(_ecm);
  this->dataPtr->FindCollisionLinks(_ecm);
}

/////////////////////////////////////////////////
void VisualizationCapabilities::LoadConfig(const tinyxml2::XMLElement *)
{
  if (this->title.empty())
    this->title = "VisualizationCapabilities";

  // view as transparent service
  this->dataPtr->viewTransparentService = "/gui/view/transparent";
  this->dataPtr->node.Advertise(this->dataPtr->viewTransparentService,
    &VisualizationCapabilitiesPrivate::OnViewTransparent, this->dataPtr.get());
  ignmsg << "View as transparent service on ["
         << this->dataPtr->viewTransparentService << "]" << std::endl;

  // view wireframes service
  this->dataPtr->viewWireframesService = "/gui/view/wireframes";
  this->dataPtr->node.Advertise(this->dataPtr->viewWireframesService,
   &VisualizationCapabilitiesPrivate::OnViewWireframes, this->dataPtr.get());
  ignmsg << "View as wireframes service on ["
        << this->dataPtr->viewWireframesService << "]" << std::endl;

  // view center of mass service
  this->dataPtr->viewCOMService = "/gui/view/com";
  this->dataPtr->node.Advertise(this->dataPtr->viewCOMService,
      &VisualizationCapabilitiesPrivate::OnViewCOM, this->dataPtr.get());
  ignmsg << "View center of mass service on ["
         << this->dataPtr->viewCOMService << "]" << std::endl;

  // view inertia service
  this->dataPtr->viewInertiaService = "/gui/view/inertia";
  this->dataPtr->node.Advertise(this->dataPtr->viewInertiaService,
      &VisualizationCapabilitiesPrivate::OnViewInertia, this->dataPtr.get());
  ignmsg << "View inertia service on ["
         << this->dataPtr->viewInertiaService << "]" << std::endl;

   // view collisions service
   this->dataPtr->viewCollisionsService = "/gui/view/collisions";
   this->dataPtr->node.Advertise(this->dataPtr->viewCollisionsService,
       &VisualizationCapabilitiesPrivate::OnViewCollisions,
       this->dataPtr.get());
   ignmsg << "View collisions service on ["
          << this->dataPtr->viewCollisionsService << "]" << std::endl;

  ignition::gui::App()->findChild
    <ignition::gui::MainWindow *>()->installEventFilter(this);
}

////////////////////////////////////////////////
bool VisualizationCapabilities::eventFilter(QObject *_obj, QEvent *_event)
{
  if (_event->type() == ignition::gui::events::Render::kType)
  {
    this->dataPtr->OnRender();
  }
  return QObject::eventFilter(_obj, _event);
}


// Register this plugin
IGNITION_ADD_PLUGIN(ignition::gazebo::VisualizationCapabilities,
                    ignition::gui::Plugin)
