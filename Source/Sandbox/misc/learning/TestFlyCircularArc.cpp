#include "TestFlyCircularArc.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/utilities/geometry.h"
#include "Sandbox/utilities/macros/null_checks.hpp"

#include <Components/SceneComponent.h>
#include <Components/StaticMeshComponent.h>
#include <Engine/World.h>

ATestFlyCircularArc::ATestFlyCircularArc()
    : mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void ATestFlyCircularArc::Tick(float dt) {
    Super::Tick(dt);

    log_config.tick(dt);

    move_to_point(dt);
    draw_shapes();

    log_config.on_tick_end();
}
void ATestFlyCircularArc::move_to_point(float dt) {
    auto const pos{GetActorLocation()};
    distance_from_target = FVector::Dist(destination, pos);

    auto const to_dest{destination - circle_centre};
    auto const to_char{pos - circle_centre};

    auto const dir_to_dest{to_dest.GetSafeNormal()};
    auto const dir_to_char{to_char.GetSafeNormal()};

    auto const max_dist{dt * speed};

    auto const cur_angle{FMath::Atan2(dir_to_char.Y, dir_to_char.X)};
    auto const dest_angle{FMath::Atan2(dir_to_dest.Y, dir_to_dest.X)};
    auto const angle_left{dest_angle - cur_angle};
    auto const abs_angle_left{FMath::Abs(angle_left)};

    auto const proportion_left{abs_angle_left / (2.f * PI)};
    auto const dist_left{proportion_left * circumfrence};

    if (FMath::IsNearlyZero(dist_left)) { SetActorTickEnabled(false); }

    auto const dist_move{FMath::Min(dist_left, max_dist)};
    auto const proportion_move{dist_move / circumfrence};

    auto const angle_move{2.f * PI * proportion_move * (angle_left < 0.f ? -1.f : 1.f)};
    auto const new_angle{cur_angle + angle_move};

    FVector const circle_pos{
        radius * FMath::Cos(new_angle),
        radius * FMath::Sin(new_angle),
        0.0,
    };

    SetActorLocation(circle_centre + circle_pos);

    if (log_config.can_log(EActorLogVerbosity::Basic)) {
        UE_LOG(LogSandboxLearning,
               Display,
               TEXT(R"(
###############################
TICK
###############################
Delta time: %.5f
Pos: %s
Dest: %s
Centre: %s
To pos: %s
To dest: %s
Dir to char: %s
Dir to dest: %s
Radius: %.2f
Centre to pos: %.2f
Centre to dest: %.2f
Max dist: %.4f
Cur angle: %.2f
Dest angle: %.2f
Angle left: %.2f
Proportion left: %.2f
Dist left: %.4f
Dist move: %.4f
Prop move: %.4f
Angle move: %.2f
New angle: %.2f
New point: %s
)"),
               dt,
               *pos.ToCompactString(),
               *destination.ToCompactString(),
               *circle_centre.ToCompactString(),
               *to_char.ToCompactString(),
               *to_dest.ToCompactString(),
               *dir_to_char.ToCompactString(),
               *dir_to_dest.ToCompactString(),
               radius,
               to_char.Size(),
               to_dest.Size(),
               max_dist,
               FMath::RadiansToDegrees(cur_angle),
               FMath::RadiansToDegrees(dest_angle),
               FMath::RadiansToDegrees(angle_left),
               proportion_left,
               dist_left,
               dist_move,
               proportion_move,
               FMath::RadiansToDegrees(angle_move),
               FMath::RadiansToDegrees(new_angle),
               *circle_pos.ToCompactString());
    }
}

void ATestFlyCircularArc::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    if (!target_actor.IsValid()) { return; }
    if (!on_same_z()) { return; }
    initial_setup(*target_actor);
}
void ATestFlyCircularArc::BeginPlay() {
    Super::BeginPlay();

    if (!target_actor.IsValid()) {
        WARN_IS_FALSE(LogSandboxLearning, target_actor);
        SetActorTickEnabled(false);
        return;
    }
    if (!on_same_z()) {
        UE_LOG(LogSandboxLearning, Warning, TEXT("Points not on same Z value"));
        SetActorTickEnabled(false);
        return;
    }

    SetActorTickEnabled(true);
    SetActorTickInterval(tick_interval);
    initial_setup(*target_actor);

    draw_config.lifetime = tick_interval;
    draw_config.world = GetWorld();
}

bool ATestFlyCircularArc::on_same_z() const {
    return FMath::IsNearlyEqual(GetActorLocation().Z, target_actor->GetActorLocation().Z);
}
void ATestFlyCircularArc::initial_setup(AActor const& tgt) {
    starting_point = GetActorLocation();
    destination = tgt.GetActorLocation();

    auto const displacement{destination - starting_point};
    auto const mid_displacement{displacement / 2.0};

    mid_point = starting_point + mid_displacement;

    distance_from_target = displacement.Size();

    auto const towards_destination{displacement.GetSafeNormal()};
    plane_normal = FVector::UpVector;
    side_direction = FVector::CrossProduct(towards_destination, plane_normal);

    // Pythagoras
    auto const o{distance_from_target / 2.f};
    auto const theta{FMath::DegreesToRadians(angle_deg / 2.f)};
    auto const a{o * FMath::Tan(theta)};
    radius = FMath::Sqrt(a * a + o * o);

    distance_from_midpoint = a;
    circle_centre = mid_point + side_direction * distance_from_midpoint;

    centre_to_mid = mid_point - circle_centre;

    circumfrence = 2.f * PI * radius;
}
void ATestFlyCircularArc::draw_shapes() const {
    TRY_INIT_PTR(world, GetWorld());

    auto const& c{draw_config};

    auto const a0{(starting_point - circle_centre).GetSafeNormal()};
    auto const a1{(destination - circle_centre).GetSafeNormal()};
    auto const angle_between{ml::get_angle_between_norm_lines(a0, a1)};

    c.draw_point(mid_point);

    c.draw_line(circle_centre, starting_point);
    c.draw_line(circle_centre, destination);
    c.draw_line(circle_centre, GetActorLocation());
    c.draw_line(starting_point, destination);

    FTransform circle_transform{
        c.get_circle_xy_rotation(),
        circle_centre,
        FVector::OneVector,
    };

    c.draw_circle(circle_transform, radius);
}
