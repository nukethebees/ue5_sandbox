// Create forwarding functions in classes
#define FORWARDING_FN(MEMBER_NAME, FN_NAME, ...)                     \
    template <typename Self, typename... Args>                       \
    decltype(auto) FN_NAME(this Self&& self, Args&&... args) {       \
        return std::forward<Self>(self).MEMBER_NAME.FN_NAME(         \
            std::forward<Args>(args)... __VA_OPT__(, ) __VA_ARGS__); \
    }
