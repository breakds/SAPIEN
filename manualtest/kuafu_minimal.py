# A minimal example of using KuafuRenderer
#
# By Jet <i@jetd.me>
#
import sapien.core as sapien
from sapien.core import Pose


def main():
    sim = sapien.Engine()

    render_config = sapien.KuafuConfig()
    render_config.spp = 64
    render_config.max_bounces = 6

    renderer = sapien.KuafuRenderer(render_config)
    sim.set_renderer(renderer)

    config = sapien.SceneConfig()
    scene = sim.create_scene(config)

    material = renderer.create_material()
    material.set_base_color([1.0, 0.5, 0.5, 1.0])
    material.set_roughness(1.0)
    scene.add_ground(0, render_material=material)
    scene.set_timestep(1 / 60)

    mount = scene.create_actor_builder().build_static()
    cam1 = scene.add_mounted_camera(
        "cam", mount, Pose(), 800, 600, 0, 1.0, 0.1, 100)
    mount.set_pose(Pose([-12, 0, 1.8]))

    builder = scene.create_actor_builder()
    material = renderer.create_material()
    material.set_base_color([0.2, 0.2, 0.8, 1.0])
    material.set_roughness(0.5)
    material.set_metallic(0.0)
    builder.add_sphere_visual(material=material)
    builder.add_sphere_collision()
    sphere1 = builder.build()
    sphere1.set_pose(Pose(p=[0, -3, 4]))

    builder = scene.create_actor_builder()
    material = renderer.create_material()
    material.set_base_color([0.8, 0.2, 0.2, 1.0])
    material.set_roughness(0.05)
    material.set_metallic(1.0)
    builder.add_sphere_visual(material=material)
    builder.add_sphere_collision()
    sphere2 = builder.build()
    sphere2.set_pose(Pose(p=[0, 3, 4]))

    builder = scene.create_actor_builder()
    material = renderer.create_material()
    material.set_transmission(1.0)
    material.set_ior(1.45)
    material.set_base_color([0.2, 0.8, 0.2, 1.0])
    # material.set_roughness(1.0)
    material.set_roughness(0.0)
    material.set_specular(0.0)
    builder.add_box_visual(material=material)
    builder.add_box_collision()
    box = builder.build()
    box.set_pose(Pose(p=[0, 0, 4]))

    scene.set_ambient_light([0.5, 0.5, 0.5])
    dirlight = scene.add_directional_light(
        [0, 0.5, -1], color=[5.0, 5.0, 5.0]
    )
    # plight = scene.add_point_light([0, 0, 0.1], [1000.0, 1000.0, 1000.0])

    cnt = 0

    while True:
        scene.step()
        scene.update_render()
        cam1.take_picture()

        cnt = (cnt + 1) % 2000

        if cnt < 1000:
            sphere1.set_velocity([0.5, 0, 0])
            sphere2.set_velocity([-0.5, 0, 0])
        else:
            sphere1.set_velocity([-0.5, 0, 0])
            sphere2.set_velocity([0.5, 0, 0])

        sphere1.set_angular_velocity([0, 0, 1])
        sphere2.set_angular_velocity([0, 0, -1])
        box.set_angular_velocity([0, 0, 1])
        # print(a.pose)
        # print(cam1.get_pose())

main()

