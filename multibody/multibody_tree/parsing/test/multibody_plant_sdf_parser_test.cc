#include "drake/multibody/multibody_tree/parsing/multibody_plant_sdf_parser.h"

#include <memory>

#include <gtest/gtest.h>
#include <sdf/sdf.hh>

#include "drake/common/find_resource.h"
#include "drake/common/test_utilities/eigen_matrix_compare.h"
#include "drake/common/test_utilities/expect_throws_message.h"
#include "drake/geometry/geometry_instance.h"
#include "drake/geometry/scene_graph.h"
#include "drake/multibody/multibody_tree/multibody_plant/multibody_plant.h"
#include "drake/systems/framework/context.h"

namespace drake {

using Eigen::Vector3d;
using geometry::GeometryId;
using geometry::GeometryInstance;
using geometry::SceneGraph;
using multibody::Body;
using multibody::parsing::AddModelFromSdfFile;
using multibody::parsing::AddModelsFromSdfFile;
using multibody::multibody_plant::MultibodyPlant;
using systems::Context;

namespace multibody {
namespace parsing {
namespace test {
namespace {

// Verifies model instances are correctly created in the plant.
GTEST_TEST(MultibodyPlantSdfParserTest, ModelInstanceTest) {
  // We start with the world and default model instances.
  MultibodyPlant<double> plant;
  ASSERT_EQ(plant.num_model_instances(), 2);

  const std::string full_name = FindResourceOrThrow(
      "drake/multibody/multibody_tree/parsing/test/"
      "links_with_visuals_and_collisions.sdf");

  ModelInstanceIndex instance1 =
      AddModelFromSdfFile(full_name, "instance1", &plant);

  // Check that a duplicate model names are not allowed.
  DRAKE_EXPECT_THROWS_MESSAGE(
      AddModelFromSdfFile(full_name, "instance1", &plant), std::logic_error,
      "This model already contains a model instance named 'instance1'. "
      "Model instance names must be unique within a given model.");

  // Load two acrobots to check per-model-instance items.
  const std::string acrobot_sdf_name = FindResourceOrThrow(
      "drake/multibody/benchmarks/acrobot/acrobot.sdf");
  ModelInstanceIndex acrobot1 =
      AddModelFromSdfFile(acrobot_sdf_name, &plant);

  // Loading the model again without specifying a different model name should
  // throw.
  EXPECT_THROW(AddModelFromSdfFile(acrobot_sdf_name, &plant),
               std::logic_error);

  ModelInstanceIndex acrobot2 =
      AddModelFromSdfFile(acrobot_sdf_name, "acrobot2", &plant);

  // We are done adding models.
  plant.Finalize();

  ASSERT_EQ(plant.num_model_instances(), 5);
  EXPECT_EQ(plant.GetModelInstanceByName("instance1"), instance1);
  EXPECT_EQ(plant.GetModelInstanceByName("acrobot"), acrobot1);
  EXPECT_EQ(plant.GetModelInstanceByName("acrobot2"), acrobot2);

  // Check a couple links from the first model without specifying the model
  // instance.
  EXPECT_TRUE(plant.HasBodyNamed("link3"));
  EXPECT_FALSE(plant.HasBodyNamed("link_which_doesnt_exist"));

  // Links which appear in multiple model instances throw if the instance
  // isn't specified.
  DRAKE_EXPECT_THROWS_MESSAGE(
      plant.HasBodyNamed("Link1"), std::logic_error,
      "Body Link1 appears in multiple model instances.");

  EXPECT_FALSE(plant.HasBodyNamed("Link1", instance1));
  EXPECT_TRUE(plant.HasBodyNamed("Link1", acrobot1));
  EXPECT_TRUE(plant.HasBodyNamed("Link1", acrobot2));

  const Body<double>& acrobot1_link1 =
      plant.GetBodyByName("Link1", acrobot1);
  const Body<double>& acrobot2_link1 =
      plant.GetBodyByName("Link1", acrobot2);
  EXPECT_NE(acrobot1_link1.index(), acrobot2_link1.index());
  EXPECT_EQ(acrobot1_link1.model_instance(), acrobot1);
  EXPECT_EQ(acrobot2_link1.model_instance(), acrobot2);

  DRAKE_EXPECT_THROWS_MESSAGE(
      plant.GetBodyByName("Link1"), std::logic_error,
      "Body Link1 appears in multiple model instances.");


  DRAKE_EXPECT_THROWS_MESSAGE(
      plant.HasJointNamed("ShoulderJoint"), std::logic_error,
      "Joint ShoulderJoint appears in multiple model instances.");
  EXPECT_FALSE(plant.HasJointNamed("ShoulderJoint", instance1));
  EXPECT_TRUE(plant.HasJointNamed("ShoulderJoint", acrobot1));
  EXPECT_TRUE(plant.HasJointNamed("ShoulderJoint", acrobot2));

  const Joint<double>& acrobot1_joint =
      plant.GetJointByName("ShoulderJoint", acrobot1);
  const Joint<double>& acrobot2_joint =
      plant.GetJointByName("ShoulderJoint", acrobot2);
  EXPECT_NE(acrobot1_joint.index(), acrobot2_joint.index());
  EXPECT_EQ(acrobot1_joint.model_instance(), acrobot1);
  EXPECT_EQ(acrobot2_joint.model_instance(), acrobot2);

  DRAKE_EXPECT_THROWS_MESSAGE(
      plant.GetJointByName("ShoulderJoint"), std::logic_error,
      "Joint ShoulderJoint appears in multiple model instances.");

  DRAKE_EXPECT_THROWS_MESSAGE(
      plant.HasJointActuatorNamed("ElbowJoint"), std::logic_error,
      "Joint actuator ElbowJoint appears in multiple model instances.");

  const JointActuator<double>& acrobot1_actuator =
      plant.GetJointActuatorByName("ElbowJoint", acrobot1);
  const JointActuator<double>& acrobot2_actuator =
      plant.GetJointActuatorByName("ElbowJoint", acrobot2);
  EXPECT_NE(acrobot1_actuator.index(), acrobot2_actuator.index());

  DRAKE_EXPECT_THROWS_MESSAGE(
      plant.GetJointActuatorByName("ElbowJoint"), std::logic_error,
      "Joint actuator ElbowJoint appears in multiple model instances.");

  const Frame<double>& acrobot1_link1_frame =
      plant.GetFrameByName("Link1", acrobot1);
  const Frame<double>& acrobot2_link1_frame =
      plant.GetFrameByName("Link1", acrobot2);
  EXPECT_NE(acrobot1_link1_frame.index(), acrobot2_link1_frame.index());

  DRAKE_EXPECT_THROWS_MESSAGE(
      plant.GetFrameByName("Link1"), std::logic_error,
      "Frame Link1 appears in multiple model instances.");
}

// Verify that our SDF parser throws an exception when a user specifies a joint
// with negative damping.
GTEST_TEST(SdfParserThrowsWhen, JointDampingIsNegative) {
  const std::string sdf_file_path =
      "drake/multibody/multibody_tree/parsing/test/negative_damping_joint.sdf";
  MultibodyPlant<double> plant;
  DRAKE_EXPECT_THROWS_MESSAGE(
      AddModelFromSdfFile(FindResourceOrThrow(sdf_file_path), &plant),
      std::runtime_error,
      /* Verify this method is throwing for the right reasons. */
      "Joint damping is negative for joint '.*'. "
          "Joint damping must be a non-negative number.");
}

GTEST_TEST(SdfParser, IncludeTags) {
  const std::string sdf_file_path =
      "drake/multibody/multibody_tree/parsing/test";
  sdf::addURIPath("model://", FindResourceOrThrow(sdf_file_path));
  MultibodyPlant<double> plant;

  // We start with the world and default model instances.
  ASSERT_EQ(plant.num_model_instances(), 2);
  ASSERT_EQ(plant.num_bodies(), 1);
  ASSERT_EQ(plant.num_joints(), 0);

  AddModelsFromSdfFile(FindResourceOrThrow(
        sdf_file_path + "/include_models.sdf"), &plant);
  plant.Finalize();

  // We should have loaded three more models.
  EXPECT_EQ(plant.num_model_instances(), 5);
  // The models should have added 8 four more bodies.
  EXPECT_EQ(plant.num_bodies(), 9);
  // The models should have added five more joints.
  EXPECT_EQ(plant.num_joints(), 5);

  // There should be a model instance with the name "robot1".
  EXPECT_TRUE(plant.HasModelInstanceNamed("robot1"));
  ModelInstanceIndex robot1_model = plant.GetModelInstanceByName("robot1");
  // There should be a body with the name "base_link".
  EXPECT_TRUE(plant.HasBodyNamed("base_link", robot1_model));
  // There should be another body with the name "moving_link".
  EXPECT_TRUE(plant.HasBodyNamed("moving_link", robot1_model));
  // There should be joint with the name "slider".
  EXPECT_TRUE(plant.HasJointNamed("slider", robot1_model));

  // There should be a model instance with the name "robot2".
  EXPECT_TRUE(plant.HasModelInstanceNamed("robot2"));
  ModelInstanceIndex robot2_model = plant.GetModelInstanceByName("robot2");

  // There should be a body with the name "base_link".
  EXPECT_TRUE(plant.HasBodyNamed("base_link", robot2_model));
  // There should be another body with the name "moving_link".
  EXPECT_TRUE(plant.HasBodyNamed("moving_link", robot2_model));
  // There should be joint with the name "slider".
  EXPECT_TRUE(plant.HasJointNamed("slider", robot2_model));

  // There should be a model instance with the name "weld_robots".
  EXPECT_TRUE(plant.HasModelInstanceNamed("weld_models"));
  ModelInstanceIndex weld_model = plant.GetModelInstanceByName("weld_models");

  // There should be all the bodies and joints contained in "simple_robot1"
  // prefixed with the model's name of "robot1".
  EXPECT_TRUE(plant.HasBodyNamed("robot1::base_link", weld_model));
  EXPECT_TRUE(plant.HasBodyNamed("robot1::moving_link", weld_model));
  EXPECT_TRUE(plant.HasJointNamed("robot1::slider", weld_model));
  // There should be all the bodies and joints contained in "simple_robot2"
  // prefixed with the model's name of "robot2".
  EXPECT_TRUE(plant.HasBodyNamed("robot2::base_link", weld_model));
  EXPECT_TRUE(plant.HasBodyNamed("robot2::moving_link", weld_model));
  EXPECT_TRUE(plant.HasJointNamed("robot2::slider", weld_model));
  // There should be a joint named "weld_robots"
  EXPECT_TRUE(plant.HasJointNamed("weld_robots", weld_model));
}

}  // namespace
}  // namespace test
}  // namespace parsing
}  // namespace multibody
}  // namespace drake
