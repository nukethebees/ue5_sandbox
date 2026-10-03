use serde::Deserialize;

#[derive(Clone, Copy, Debug, Deserialize)]
pub enum TimeUnit {
    #[serde(rename = "ns")]
    Nanoseconds,
    #[serde(rename = "us")]
    Microseconds,
    #[serde(rename = "ms")]
    Milliseconds,
    #[serde(rename = "s")]
    Seconds,
}

impl TimeUnit {
    pub fn to_nanoseconds(self, value: f64) -> f64 {
        value
            * match self {
                Self::Nanoseconds => 1.0,
                Self::Microseconds => 1e3,
                Self::Milliseconds => 1e6,
                Self::Seconds => 1e9,
            }
    }
}
