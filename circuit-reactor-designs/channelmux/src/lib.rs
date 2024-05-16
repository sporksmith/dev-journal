use std::collections::HashMap;
use std::hash::{Hash, RandomState};
use std::pin::Pin;
use std::task::{Context, Poll};

use futures::Stream;
use indexmap::IndexMap;

pub struct ChannelMux<K, V, S = RandomState> {
    /// The "data" receiver for each key.
    channels: HashMap<K, futures::channel::mpsc::Receiver<V>, S>,
    /// We receive notifications here each time data is pushed into a channel.
    /// This allows us to track which receivers are ready without having to do
    /// linear scans over all receivers.
    notifications_recv: Pin<Box<futures::channel::mpsc::Receiver<K>>>,
    /// We keep a sender-side to the notification channel. When a channel
    /// is added to the set, we clone this.
    notifications_send: futures::channel::mpsc::Sender<K>,
    /// Track the number of messages available in each data channel. 
    /// Values are incremented when notifications are read from `notifications_recv`,
    /// and decremented when data objects are read out of `channels`.
    /// When a value is read out of a data channel, the corresponding key here
    /// is removed and reinserted
    /// into the map, so that it becomes last in iteration order, providing
    /// fairness.
    notifications: IndexMap<K, usize>,
}

impl <K, V, S> ChannelMux<K, V, S> {
    /// Add a new key to the channel set, returning the sender side for the channel.
    pub fn add_key(&mut self, key: K, bound: usize) -> ChannelMuxSender<K, V> { todo!() }
}

impl <K, V, S> futures::Stream for ChannelMux<K, V, S> where K: Hash + Eq {
    type Item=(K, V);

    fn poll_next(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Option<Self::Item>> {
        loop {
            match self.notifications_recv.as_mut().poll_next(cx) {
                Poll::Ready(Some(k)) => {
                    match self.notifications.entry(k) {
                        indexmap::map::Entry::Occupied(mut o) => {*o.get_mut() += 1; },
                        indexmap::map::Entry::Vacant(v) => {v.insert(1);},
                    };
                },
                Poll::Ready(None) => // We keep a clone of the sender, and never close it.
                unreachable!("Channel unexpectedly closed"),
                Poll::Pending => break,
            }
        }
        todo!()
       /*
          * read all available notifications from self.notifications_recv, updating self.notifications,
            and implicitly registering to be woken when there are new notifications.
            (or just read once, but eagerly reading here lets us "compress" multiple notifications
            from the same sender)
          * if self.notifications is non-empty:
            * remove the first entry from self.notifications
            * if the value is > 1, reinsert the key to the end, with value decremented
            * read from self.channels[k], which we know is ready.
              XXX: Not sure we can guarantee the order that the writers complete.
              If not, and if the data isn't actually ready yet,
              then we probably want to save this notification, but move on to the next,
              whose data might be ready.
            * return Poll::Ready((K, V))
          * else
            * return Poll::Pending
       */
    }
}

struct ChannelMuxSender<K, V> {
    /// Our key
    key: K,
    /// We send notifications here when we write to the data channel.
    notifications_send: futures::channel::mpsc::Sender<K>,
    /// The data channel.
    data_send: futures::channel::mpsc::Sender<V>,
}

impl <K, V> futures::Sink<V> for ChannelMuxSender<K, V> {
    fn poll_ready(
        self: Pin<&mut Self>,
        cx: &mut Context<'_>
    ) -> Poll<Result<(), Self::Error>> {
      // Only need to check the data channel, since the
      // notification channel is unbounded.
      // XXX verify this is true
      self.data_send.poll_ready(cx)
    }

    fn start_send(self: Pin<&mut Self>, item: Item) -> Result<(), Self::Error> {
      self.data_send.start_send(item)?;
      self.notifications_send.start_send(key)
    }

    fn poll_flush(
        self: Pin<&mut Self>,
        cx: &mut Context<'_>
    ) -> Poll<Result<(), Self::Error>> {
      match self.data_send.poll_flush() {
        Poll::Ready(Ok(())) => (),
        Poll::Ready(Err(e)) => return Poll::Ready(Err(e)),
        Poll::Pending => return Poll::Pending,
      };

      // XXX I *think* this is guaranteed to be ready
      // since the channel is unbounded?
      self.notifications_send.poll_flush()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn it_works() {
    }
}
