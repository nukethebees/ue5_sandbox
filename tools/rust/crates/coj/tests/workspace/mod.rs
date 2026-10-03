use super::*;

#[test]
fn branch_comparison_follows_platform_semantics() {
    assert!(same_branch("feature/topic", "feature/topic"));
    assert_eq!(same_branch("Feature/Topic", "feature/topic"), cfg!(windows));
    assert!(!same_branch("feature/a", "feature/b"));
}
