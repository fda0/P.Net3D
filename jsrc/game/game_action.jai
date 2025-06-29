Action :: struct
{
  type: enum
  {
    IDLE;
    PATHING;
    PATHING_DIRECTION;
    ATTACK;
  };

  pressed_timestamp: TimestampMS;
  world_p: V3; // used by PATHING*
  obj_key: OBJ_Key; // used by ATTACK
}
