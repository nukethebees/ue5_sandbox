#pragma once

#include <concepts>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

template <typename T>
concept IsPoolableActor = std::derived_from<T, AActor> && requires(T* t) {
    { t->Activate() } -> std::same_as<void>;
    { t->Deactivate() } -> std::same_as<void>;
};

template <typename T>
concept IsPoolConfig = requires {
    typename T::ActorType;
    requires IsPoolableActor<typename T::ActorType>;
    { T::DefaultPoolSize } -> std::convertible_to<int32>;
    { T::MaxPoolSize } -> std::convertible_to<std::optional<int32>>;
    { T::GetDefaultClass() } -> std::same_as<TSubclassOf<typename T::ActorType>>;
};
