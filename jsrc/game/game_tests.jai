#run TEST_run();

TEST_run :: ()
{
  TEST_util();
}

TEST_util :: ()
{
  q: Queue(4, u32);

  QueuePush(*q, 11);
  QueuePush(*q, 22);
  QueuePush(*q, 33); // not full (3/4)
  sum: u32; for q sum += it;
  assert(sum == 66);

  QueuePush(*q, 50); // full
  sum = 0; for q sum += it;
  assert(sum == 116);

  QueuePush(*q).* = 80; // removed 11; added 80
  sum = 0; for q sum += it;
  assert(sum == 185);

  popped := QueuePop(*q); // removed 22
  assert(popped == 22);
  sum = 0; for q sum += it;
  assert(sum == 163);

  peeked := QueuePeek(q);
  assert(peeked == 33);

  peeked_pointer := QueuePeekPointer(*q);
  assert(peeked_pointer.* == 33);
}
